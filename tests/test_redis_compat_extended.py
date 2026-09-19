import threading
import time


def test_redis_compat_extended(port=6389):
    import redis

    client = redis.Redis(host="127.0.0.1", port=port, decode_responses=True, socket_timeout=3)
    client.flushdb()

    # --- Hash operations ---
    assert client.hset("call:session:1", mapping={"status": "ringing", "p:user": "joined"}) == 2
    assert client.hsetnx("call:session:1", "status", "active") == 0
    assert client.hget("call:session:1", "status") == "ringing"
    assert client.hgetall("call:session:1")["p:user"] == "joined"
    assert client.hlen("call:session:1") == 2
    assert client.hdel("call:session:1", "p:user") == 1
    assert client.hdel("call:session:1", "nonexistent") == 0
    assert client.hlen("call:session:1") == 1

    # Empty collection TTL cleanup
    client.hset("temp:hash", "f1", "v1")
    client.expire("temp:hash", 60)
    assert client.hdel("temp:hash", "f1") == 1
    assert client.exists("temp:hash") == 0
    assert client.ttl("temp:hash") == -2

    # --- Set operations ---
    assert client.sadd("call:voip:1", "APNS|token", "FCM|token2") == 2
    assert client.srem("call:voip:1", "FCM|missing") == 0
    assert client.smembers("call:voip:1") == {"APNS|token", "FCM|token2"}

    # --- Sorted Set operations including ZSCORE ---
    assert client.zadd("call:deadlines", {"session|user": 10.5, "later|user": 20.25}) == 2
    assert client.zscore("call:deadlines", "later|user") == 20.25
    assert client.zscore("call:deadlines", "nonexistent") is None
    assert client.zrangebyscore("call:deadlines", 0, 11) == ["session|user"]
    assert client.zrem("call:deadlines", "session|user") == 1
    assert client.zscore("call:deadlines", "session|user") is None

    # --- Expire & Glob Pattern Matching ---
    assert client.expire("call:session:1", 10) is True
    assert client.ttl("call:session:1") > 0
    assert "call:session:1" in set(client.scan_iter(match="call:session:*"))

    client.set("user:101:profile", "val1")
    client.set("user:202:profile", "val2")
    client.set("user:101:settings", "val3")
    keys_matched = set(client.keys("user:*:profile"))
    assert keys_matched == {"user:101:profile", "user:202:profile"}, f"Glob match failed: {keys_matched}"

    # --- Concurrent Non-blocking BRPOP ---
    popped_1 = []
    popped_2 = []

    def worker(key, out_list):
        q = redis.Redis(host="127.0.0.1", port=port, decode_responses=True, socket_timeout=4)
        out_list.append(q.brpop(key, timeout=3))
        q.close()

    t1 = threading.Thread(target=worker, args=("queue_a", popped_1))
    t2 = threading.Thread(target=worker, args=("queue_b", popped_2))
    t1.start()
    t2.start()
    time.sleep(0.15)

    # Producer pushes to both queues while workers are blocked
    assert client.lpush("queue_a", "payload_a") == 1
    assert client.lpush("queue_b", "payload_b") == 1

    t1.join(timeout=4)
    t2.join(timeout=4)

    assert popped_1 == [("queue_a", "payload_a")], f"worker 1 got {popped_1}"
    assert popped_2 == [("queue_b", "payload_b")], f"worker 2 got {popped_2}"

    # --- Pub/Sub with exact channel name and pattern matching ---
    pubsub = client.pubsub()
    pubsub.psubscribe("events:*")
    subscription = pubsub.get_message(timeout=2)
    assert subscription and subscription["type"] == "psubscribe"
    assert client.publish("events:user-1", "hello") == 1
    message = pubsub.get_message(timeout=2)
    assert message and message["type"] == "pmessage"
    assert message["channel"] == "events:user-1"
    assert message["data"] == "hello"

    # Direct subscribe
    pubsub2 = client.pubsub()
    pubsub2.subscribe("direct_chan")
    sub2_ack = pubsub2.get_message(timeout=2)
    assert sub2_ack and sub2_ack["type"] == "subscribe"
    assert sub2_ack["channel"] == "direct_chan"
    assert client.publish("direct_chan", "msg_content") == 1
    m2 = pubsub2.get_message(timeout=2)
    assert m2 and m2["data"] == "msg_content"

    pubsub.close()
    pubsub2.close()
    client.close()
    print("[PASS] Extended Redis compatibility tests passed!")
