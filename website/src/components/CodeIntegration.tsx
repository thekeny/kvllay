import { useState } from "react"
import { IconCheck, IconCopy } from "@tabler/icons-react"
import { paths } from "@/config/paths"
import { Reveal } from "@/components/ui/Reveal"

export function CodeIntegration() {
  const [activeTab, setActiveTab] = useState<"python" | "node" | "go">("python")
  const [copied, setCopied] = useState(false)

  const sdks = [
    {
      title: "Python (redis-py, redis-om)",
      desc: "Full sync & async pipeline support with standard redis.Redis() client connection.",
    },
    {
      title: "Node.js & TypeScript (ioredis, redis)",
      desc: "Zero-config compatibility with Express, NestJS, Fastify, Next.js cache adapters.",
    },
    {
      title: "Go (go-redis/v9, redigo)",
      desc: "High-throughput connection pooling with transparent failover and context timeouts.",
    },
    {
      title: "Rust, PHP, C#, Java & redis-cli",
      desc: "Works with fred, predis, Predis/PhpRedis, StackExchange.Redis, Jedis, and CLI tools out of the box.",
    },
  ]

  const snippets = {
    python: `import redis

client = redis.Redis(host='localhost', port=6379, decode_responses=True)

# 1. Standard String caching with TTL
client.set("session:token", "jwt_payload", ex=3600)
val = client.get("session:token")

# 2. Atomic Transaction (MULTI / EXEC)
pipe = client.pipeline(transaction=True)
pipe.set("user:101:status", "active")
pipe.incr("user:101:visits")
res = pipe.execute()
print(f"User: {val}, Tx Committed: {res}")`,

    node: `import Redis from 'ioredis';

const redis = new Redis({ host: '127.0.0.1', port: 6379 });

// 1. Standard String caching with TTL
await redis.set('cache:products', JSON.stringify([{ id: 1 }]), 'EX', 600);
const data = await redis.get('cache:products');

// 2. Atomic Pipeline Transaction (MULTI / EXEC)
const txResult = await redis
  .multi()
  .set('user:session', 'active')
  .incr('analytics:pageviews')
  .exec();

console.log('Products:', JSON.parse(data), 'Tx:', txResult);`,

    go: `package main

import (
    "context"
    "fmt"
    "time"
    "github.com/redis/go-redis/v9"
)

func main() {
    ctx := context.Background()
    rdb := redis.NewClient(&redis.Options{
        Addr: "localhost:6379",
    })

    // 1. Set key with TTL
    err := rdb.Set(ctx, "app:status", "online", 10*time.Minute).Err()
    if err != nil { panic(err) }

    // 2. Atomic TxPipeline (MULTI / EXEC)
    pipe := rdb.TxPipeline()
    pipe.Set(ctx, "user:101:status", "active", 0)
    pipe.Incr(ctx, "user:101:logins")
    _, err = pipe.Exec(ctx)
    if err != nil { panic(err) }

    fmt.Println("kvllay transaction committed successfully")
}`,
  }

  const handleCopy = () => {
    navigator.clipboard.writeText(snippets[activeTab])
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  return (
    <section id={paths.sections.integrations} className="w-full flex flex-col items-center py-10 sm:py-16 px-4 sm:px-6 lg:px-10 max-w-[1240px] mx-auto scroll-mt-20">
      <Reveal direction="up" className="flex flex-col items-center text-center gap-2.5 sm:gap-3 mb-8 sm:mb-12">
        <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full bg-[#00D2FF]/10 border border-[#00D2FF]/30">
          <span className="text-[11px] font-mono font-bold text-[#00D2FF] tracking-wider uppercase">
            DEVELOPER INTEGRATION
          </span>
        </div>
        <h2 className="text-2xl sm:text-3xl lg:text-[38px] font-bold text-[#F0F6FC] tracking-tight">
          Zero Migration Effort. Works with Any Redis SDK.
        </h2>
        <p className="text-sm sm:text-base text-[#8B9BB4] max-w-[780px] leading-[1.6]">
          Kvllay speaks Redis RESP2 natively. Keep using your favorite drivers, ORMs, and libraries without downloading proprietary wrappers.
        </p>
      </Reveal>

      <div className="w-full grid grid-cols-1 md:grid-cols-12 gap-6 md:gap-10 items-center">
        <Reveal direction="left" delay={60} className="md:col-span-5 flex flex-col gap-4 sm:gap-5">
          {sdks.map((item) => (
            <div key={item.title} className="flex items-start gap-3 sm:gap-3.5">
              <div className="w-7.5 sm:w-8 h-7.5 sm:h-8 rounded-lg bg-[#0E131F] border border-[#1B2436] flex items-center justify-center shrink-0 mt-0.5">
                <IconCheck className="size-4 text-[#00D2FF]" />
              </div>
              <div>
                <h4 className="text-[14px] sm:text-[15px] font-semibold text-[#F0F6FC] mb-0.5 sm:mb-1">{item.title}</h4>
                <p className="text-[12.5px] sm:text-[13px] text-[#8B9BB4] leading-[1.5]">{item.desc}</p>
              </div>
            </div>
          ))}
        </Reveal>

        <Reveal direction="right" delay={120} className="md:col-span-7 w-full bg-[#0B0E17] border border-[#1E3B5C] rounded-xl sm:rounded-2xl overflow-hidden shadow-[0_20px_50px_rgba(0,0,0,0.5)]">
          <div className="flex items-center justify-between px-3 sm:px-4 py-2 sm:py-2.5 bg-[#0A0E17] border-b border-[#1B2436]">
            <div className="flex items-center gap-1 sm:gap-1.5 overflow-x-auto scrollbar-none py-0.5">
              <button
                type="button"
                onClick={() => setActiveTab("python")}
                className={`px-2.5 sm:px-3 py-1 sm:py-1.5 rounded-lg text-xs font-mono font-medium transition-all ${
                  activeTab === "python"
                    ? "bg-[#0E131F] text-[#00D2FF] border border-[#00D2FF]/60 shadow-sm"
                    : "text-[#8B9BB4] hover:text-[#F0F6FC]"
                }`}
              >
                Python (redis-py)
              </button>
              <button
                type="button"
                onClick={() => setActiveTab("node")}
                className={`px-2.5 sm:px-3 py-1 sm:py-1.5 rounded-lg text-xs font-mono font-medium transition-all ${
                  activeTab === "node"
                    ? "bg-[#0E131F] text-[#00D2FF] border border-[#00D2FF]/60 shadow-sm"
                    : "text-[#8B9BB4] hover:text-[#F0F6FC]"
                }`}
              >
                Node.js (ioredis)
              </button>
              <button
                type="button"
                onClick={() => setActiveTab("go")}
                className={`px-2.5 sm:px-3 py-1 sm:py-1.5 rounded-lg text-xs font-mono font-medium transition-all ${
                  activeTab === "go"
                    ? "bg-[#0E131F] text-[#00D2FF] border border-[#00D2FF]/60 shadow-sm"
                    : "text-[#8B9BB4] hover:text-[#F0F6FC]"
                }`}
              >
                Go (go-redis)
              </button>
            </div>

            <button
              type="button"
              onClick={handleCopy}
              className="p-1.5 text-[#546682] hover:text-[#00D2FF] rounded-md transition-colors flex items-center gap-1.5 focus:outline-none shrink-0"
              title="Copy code"
            >
              {copied ? (
                <>
                  <IconCheck className="size-4 text-[#00D2FF]" />
                  <span className="text-[11px] text-[#00D2FF] font-sans font-medium hidden sm:inline">Copied!</span>
                </>
              ) : (
                <IconCopy className="size-4" />
              )}
            </button>
          </div>

          <div className="p-3.5 sm:p-5 overflow-x-auto font-mono text-xs sm:text-[13px] leading-[1.75]">
            {activeTab === "python" && (
              <div className="flex flex-col gap-0.5 min-w-[440px] sm:min-w-[480px]">
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">1</span><span><span className="text-[#C084FC]">import</span> <span className="text-[#38BDF8]">redis</span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">2</span><span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">3</span><span><span className="text-[#F0F6FC]">client</span> <span className="text-[#94A3B8]">=</span> <span className="text-[#38BDF8]">redis</span>.<span className="text-[#60A5FA]">Redis</span>(<span className="text-[#93C5FD]">host</span><span className="text-[#94A3B8]">=</span><span className="text-[#34D399]">'localhost'</span>, <span className="text-[#93C5FD]">port</span><span className="text-[#94A3B8]">=</span><span className="text-[#FBBF24]">6379</span>)</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">4</span><span><span className="text-[#F0F6FC]">client</span>.<span className="text-[#60A5FA]">set</span>(<span className="text-[#34D399]">"session:token"</span>, <span className="text-[#34D399]">"jwt_payload"</span>, <span className="text-[#93C5FD]">ex</span><span className="text-[#94A3B8]">=</span><span className="text-[#FBBF24]">3600</span>)</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">5</span><span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">6</span><span><span className="text-[#F0F6FC]">val</span> <span className="text-[#94A3B8]">=</span> <span className="text-[#F0F6FC]">client</span>.<span className="text-[#60A5FA]">get</span>(<span className="text-[#34D399]">"session:token"</span>)</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">7</span><span><span className="text-[#F0F6FC]">ttl</span> <span className="text-[#94A3B8]">=</span> <span className="text-[#F0F6FC]">client</span>.<span className="text-[#60A5FA]">ttl</span>(<span className="text-[#34D399]">"session:token"</span>)</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">8</span><span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">9</span><span><span className="text-[#60A5FA]">print</span>(<span className="text-[#34D399]">f"User: &#123;</span><span className="text-[#F0F6FC]">val</span>.<span className="text-[#60A5FA]">decode</span>()<span className="text-[#34D399]">&#125; (Remaining TTL: &#123;</span><span className="text-[#F0F6FC]">ttl</span><span className="text-[#34D399]">&#125;s)"</span>)</span></div>
              </div>
            )}

            {activeTab === "node" && (
              <div className="flex flex-col gap-0.5 min-w-[440px] sm:min-w-[480px]">
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">1</span><span><span className="text-[#C084FC]">import</span> <span className="text-[#38BDF8]">Redis</span> <span className="text-[#C084FC]">from</span> <span className="text-[#34D399]">'ioredis'</span>;</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">2</span><span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">3</span><span><span className="text-[#C084FC]">const</span> <span className="text-[#F0F6FC]">redis</span> <span className="text-[#94A3B8]">=</span> <span className="text-[#C084FC]">new</span> <span className="text-[#38BDF8]">Redis</span>(&#123; <span className="text-[#93C5FD]">host</span>: <span className="text-[#34D399]">'127.0.0.1'</span>, <span className="text-[#93C5FD]">port</span>: <span className="text-[#FBBF24]">6379</span> &#125;);</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">4</span><span><span className="text-[#C084FC]">await</span> <span className="text-[#F0F6FC]">redis</span>.<span className="text-[#60A5FA]">set</span>(<span className="text-[#34D399]">'cache:products'</span>, <span className="text-[#38BDF8]">JSON</span>.<span className="text-[#60A5FA]">stringify</span>([&#123; <span className="text-[#93C5FD]">id</span>: <span className="text-[#FBBF24]">1</span> &#125;]), <span className="text-[#34D399]">'EX'</span>, <span className="text-[#FBBF24]">600</span>);</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">5</span><span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">6</span><span><span className="text-[#C084FC]">const</span> <span className="text-[#F0F6FC]">data</span> <span className="text-[#94A3B8]">=</span> <span className="text-[#C084FC]">await</span> <span className="text-[#F0F6FC]">redis</span>.<span className="text-[#60A5FA]">get</span>(<span className="text-[#34D399]">'cache:products'</span>);</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">7</span><span><span className="text-[#38BDF8]">console</span>.<span className="text-[#60A5FA]">log</span>(<span className="text-[#34D399]">'Products:'</span>, <span className="text-[#38BDF8]">JSON</span>.<span className="text-[#60A5FA]">parse</span>(<span className="text-[#F0F6FC]">data</span>));</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">8</span><span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">9</span><span><span className="text-[#C084FC]">const</span> <span className="text-[#F0F6FC]">keys</span> <span className="text-[#94A3B8]">=</span> <span className="text-[#C084FC]">await</span> <span className="text-[#F0F6FC]">redis</span>.<span className="text-[#60A5FA]">keys</span>(<span className="text-[#34D399]">'cache:*'</span>);</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">10</span><span><span className="text-[#38BDF8]">console</span>.<span className="text-[#60A5FA]">log</span>(<span className="text-[#34D399]">`Active keys: $&#123;</span><span className="text-[#F0F6FC]">keys</span>.<span className="text-[#93C5FD]">length</span><span className="text-[#34D399]">&#125;`</span>);</span></div>
              </div>
            )}

            {activeTab === "go" && (
              <div className="flex flex-col gap-0.5 min-w-[440px] sm:min-w-[480px]">
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">1</span><span><span className="text-[#C084FC]">package</span> <span className="text-[#F0F6FC]">main</span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">2</span><span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">3</span><span><span className="text-[#C084FC]">import</span> (</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">4</span><span className="pl-4"><span className="text-[#34D399]">"context"</span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">5</span><span className="pl-4"><span className="text-[#34D399]">"fmt"</span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">6</span><span className="pl-4"><span className="text-[#34D399]">"time"</span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">7</span><span className="pl-4"><span className="text-[#34D399]">"github.com/redis/go-redis/v9"</span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">8</span><span>)</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">9</span><span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">10</span><span><span className="text-[#C084FC]">func</span> <span className="text-[#60A5FA]">main</span>() &#123;</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">11</span><span className="pl-4"><span className="text-[#F0F6FC]">ctx</span> <span className="text-[#94A3B8]">:=</span> <span className="text-[#38BDF8]">context</span>.<span className="text-[#60A5FA]">Background</span>()</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">12</span><span className="pl-4"><span className="text-[#F0F6FC]">rdb</span> <span className="text-[#94A3B8]">:=</span> <span className="text-[#38BDF8]">redis</span>.<span className="text-[#60A5FA]">NewClient</span>(&<span className="text-[#38BDF8]">redis</span>.<span className="text-[#A78BFA]">Options</span>&#123;</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">13</span><span className="pl-8"><span className="text-[#93C5FD]">Addr</span>: <span className="text-[#34D399]">"localhost:6379"</span>,</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">14</span><span className="pl-4">&#125;)</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">15</span><span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">16</span><span className="pl-4"><span className="text-[#F0F6FC]">err</span> <span className="text-[#94A3B8]">:=</span> <span className="text-[#F0F6FC]">rdb</span>.<span className="text-[#60A5FA]">Set</span>(<span className="text-[#F0F6FC]">ctx</span>, <span className="text-[#34D399]">"app:status"</span>, <span className="text-[#34D399]">"online"</span>, <span className="text-[#FBBF24]">10</span>*<span className="text-[#38BDF8]">time</span>.<span className="text-[#93C5FD]">Minute</span>).<span className="text-[#60A5FA]">Err</span>()</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">17</span><span className="pl-4"><span className="text-[#C084FC]">if</span> <span className="text-[#F0F6FC]">err</span> <span className="text-[#94A3B8]">!=</span> <span className="text-[#C084FC]">nil</span> &#123; <span className="text-[#60A5FA]">panic</span>(<span className="text-[#F0F6FC]">err</span>) &#125;</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">18</span><span></span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">19</span><span className="pl-4"><span className="text-[#F0F6FC]">val</span>, <span className="text-[#94A3B8]">_</span> <span className="text-[#94A3B8]">:=</span> <span className="text-[#F0F6FC]">rdb</span>.<span className="text-[#60A5FA]">Get</span>(<span className="text-[#F0F6FC]">ctx</span>, <span className="text-[#34D399]">"app:status"</span>).<span className="text-[#60A5FA]">Result</span>()</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">20</span><span className="pl-4"><span className="text-[#38BDF8]">fmt</span>.<span className="text-[#60A5FA]">Println</span>(<span className="text-[#34D399]">"kvllay status:"</span>, <span className="text-[#F0F6FC]">val</span>)</span></div>
                <div className="flex gap-3 text-xs"><span className="select-none text-[#546682] w-5 text-right shrink-0">21</span><span>&#125;</span></div>
              </div>
            )}
          </div>
        </Reveal>
      </div>
    </section>
  )
}
