import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from common import running_server


def test_hashes(port=6389):
    import redis

    def client(target=port):
        return redis.Redis(port=target, decode_responses=True, socket_timeout=5)

    def error(r, args, message):
        try:
            r.execute_command(*args)
        except redis.exceptions.ResponseError as exc:
            assert message in str(exc), str(exc)
        else:
            raise AssertionError(f"Expected error for {args}")

    with client() as r:
        r.flushdb()
        assert not r.hexists('hash', 'missing')
        assert r.hmget('hash', ['a', 'b']) == [None, None]
        assert r.hkeys('hash') == []
        assert r.execute_command('hMsEt', 'hash', 'a', '1', 'empty', '', 'a', '2')
        assert r.hmget('hash', ['a', 'missing', 'empty', 'a']) == ['2', None, '', '2']
        assert r.hexists('hash', 'empty')
        assert not r.hexists('hash', 'missing')
        assert set(r.hkeys('hash')) == {'a', 'empty'}
        r.pexpire('hash', 60000)
        assert r.hincrby('hash', 'a', -3) == -1
        assert r.hincrby('hash', 'new', 0) == 0
        assert r.execute_command('HMSET', 'hash', 'a', '7')
        assert 0 < r.pttl('hash') <= 60000
        for invalid in ['', 'abc', '1.5', '+1', '01', '-0', ' 1', '9223372036854775808']:
            r.hset('hash', 'bad', invalid)
            error(r, ['HINCRBY', 'hash', 'bad', '1'], 'hash value is not an integer')
            assert r.hget('hash', 'bad') == invalid
            error(r, ['HINCRBY', 'absent', 'f', invalid], 'integer or out of range')
            assert not r.exists('absent')
        for bound, delta in [(2**63 - 1, 1), (-2**63, -1)]:
            r.hset('hash', 'bound', str(bound))
            error(r, ['HINCRBY', 'hash', 'bound', delta], 'overflow')
            assert r.hget('hash', 'bound') == str(bound)
        assert r.hincrby('limits', 'min', -2**63) == -2**63
        assert r.hincrby('limits', 'max', 2**63 - 1) == 2**63 - 1
        r.set('scalar', 'value')
        commands = [('HEXISTS', 'f'), ('HINCRBY', 'f', '1'), ('HMGET', 'f'),
                    ('HMSET', 'f', 'v'), ('HKEYS',)]
        for command, *args in commands:
            error(r, [command, 'scalar', *args], 'WRONGTYPE')
            error(r, [command], 'wrong number of arguments')
        for args in [('HMSET', 'x'), ('HMSET', 'x', 'f'), ('HMSET', 'x', 'f', 'v', 'g'),
                     ('HMGET', 'x'), ('HKEYS', 'x', 'extra'), ('HEXISTS', 'x', 'f', 'extra'),
                     ('HINCRBY', 'x', 'f'), ('HINCRBY', 'x', 'f', '1', 'extra')]:
            error(r, args, 'wrong number of arguments')
        for command, *args in commands:
            r.set('expired', 'old', px=1)
            time.sleep(0.01)
            result = r.execute_command(command, 'expired', *args)
            if command in ('HINCRBY', 'HMSET'):
                assert r.type('expired') == 'hash'
                assert r.pttl('expired') == -1
            else:
                assert not result or result == [None]
        with r.pipeline() as pipe:
            pipe.execute_command('HMSET', 'tx:hash', 'n', '3')
            pipe.hincrby('tx:hash', 'n', 4)
            pipe.hmget('tx:hash', ['n', 'missing'])
            pipe.hexists('tx:hash', 'n')
            pipe.hkeys('tx:hash')
            assert pipe.execute() == [True, 7, ['7', None], True, ['n']]

        def increment(_):
            with client() as worker:
                with worker.pipeline(transaction=False) as pipe:
                    for _ in range(100):
                        pipe.hincrby('counter:hash', 'n', 1)
                    return pipe.execute()

        with ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(increment, range(8)))
        assert sorted(value for batch in results for value in batch) == list(range(1, 801))
        assert r.hget('counter:hash', 'n') == '800'

    with tempfile.TemporaryDirectory() as directory:
        args = ['--aof', str(Path(directory) / 'hash.aof'), '--appendfsync', 'always']
        target = port + 12
        with running_server(target, args), client(target) as r:
            assert r.execute_command('hmset', 'persist', 'n', '10', 'empty', '')
            assert r.hincrby('persist', 'n', -3) == 7
            r.pexpire('persist', 60000)
            error(r, ['HINCRBY', 'persist', 'empty', '1'], 'hash value')
        with running_server(target, args), client(target) as r:
            assert r.hmget('persist', ['n', 'empty']) == ['7', '']
            assert 0 < r.pttl('persist') <= 60000
            r.bgrewriteaof()
            deadline = time.monotonic() + 5
            while r.info('persistence')['aof_rewrite_in_progress']:
                assert time.monotonic() < deadline
                time.sleep(0.01)
            assert r.execute_command('HMSET', 'persist', 'extra', 'v')
            assert r.hincrby('persist', 'n', 2) == 9
        with running_server(target, args), client(target) as r:
            assert r.hmget('persist', ['n', 'extra', 'empty']) == ['9', 'v', '']
            assert 0 < r.pttl('persist') <= 60000
    print('[PASS] Hash commands, errors, TTL, transactions, concurrency and AOF replay/rewrite')
