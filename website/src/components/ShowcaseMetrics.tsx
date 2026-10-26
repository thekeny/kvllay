import { IconBox, IconBolt, IconCpu, IconGauge } from "@tabler/icons-react"
import { Reveal } from "@/components/ui/Reveal"

export function ShowcaseMetrics() {
  const metrics = [
    {
      icon: IconBox,
      value: "1.6 MB",
      title: "Docker Container",
      comparison: "Redis is ~140 MB (87x smaller)",
    },
    {
      icon: IconBolt,
      value: "3.2 ms",
      title: "Cold Start Time",
      comparison: "Redis takes ~7.6 ms (2.4x faster)",
    },
    {
      icon: IconCpu,
      value: "4.1 MB",
      title: "Idle RAM Usage",
      comparison: "Redis uses ~15.2 MB (3.7x lighter)",
    },
    {
      icon: IconGauge,
      value: "5.67M req/s",
      title: "Peak Throughput",
      comparison: "p50 latency 10 μs (~5x vs baseline Redis)",
    },
  ]

  return (
    <section className="w-full flex flex-col items-center pt-1 sm:pt-2 pb-10 sm:pb-16 px-4 sm:px-6 lg:px-10 max-w-[1240px] mx-auto">
      <Reveal direction="up" delay={0} className="w-full max-w-[960px] mb-6 sm:mb-9">
        <div className="w-full bg-surface-subtle rounded-xl overflow-hidden">
          <div className="flex items-center justify-between px-3.5 sm:px-5 py-2 sm:py-3 bg-surface-subtle">
            <div className="flex items-center gap-2">
              <span className="w-[10px] sm:w-[11px] h-[10px] sm:h-[11px] rounded-full bg-[#EF4444] inline-block" />
              <span className="w-[10px] sm:w-[11px] h-[10px] sm:h-[11px] rounded-full bg-[#F59E0B] inline-block" />
              <span className="w-[10px] sm:w-[11px] h-[10px] sm:h-[11px] rounded-full bg-[#10B981] inline-block" />
            </div>

            <div className="text-[13px] tabular-nums text-text-secondary select-none tracking-wide truncate px-2">
              redis-cli — kvllay v1.0.0 (127.0.0.1:6379)
            </div>

            <div/>
          </div>

          <code className="block p-3.5 sm:px-6 sm:py-5 font-mono text-[13px] leading-[1.75] sm:leading-[1.8] flex flex-col gap-2 sm:gap-2.5 overflow-x-auto">
            <span className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-[#00E5A3] font-bold select-none">user@edge:~$</span>
              <span className="text-text-primary">redis-cli -p 6379</span>
            </span>

            <span className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-text-secondary select-none">127.0.0.1:6379&gt;</span>
              <span className="text-text-primary font-semibold">PING</span>
              <span className="text-text-secondary select-none">:</span>
              <span className="text-[#00E5A3] font-bold">PONG</span>
            </span>

            <span className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-text-secondary select-none">127.0.0.1:6379&gt;</span>
              <span className="text-text-primary">SET user:101 '{'{"name":"Alex","role":"dev"}'}'</span>
              <span className="text-text-secondary select-none">:</span>
              <span className="text-[#00E5A3] font-bold">OK</span>
            </span>

            <span className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-text-secondary select-none">127.0.0.1:6379&gt;</span>
              <span className="text-text-primary">GET user:101</span>
              <span className="text-text-secondary select-none">:</span>
              <span className="text-[#38BDF8]">"&#123;\"name\":\"Alex\",\"role\":\"dev\"&#125;"</span>
            </span>

            <span className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-text-secondary select-none">127.0.0.1:6379&gt;</span>
              <span className="text-text-primary">SETEX session:token 3600 "active"</span>
              <span className="text-text-secondary select-none">:</span>
              <span className="text-[#00E5A3] font-bold">OK</span>
            </span>

            <span className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-text-secondary select-none">127.0.0.1:6379&gt;</span>
              <span className="text-text-primary">TTL session:token</span>
              <span className="text-text-secondary select-none">:</span>
              <span className="text-[#FBBF24]">(integer) 3600</span>
            </span>

            <span className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-text-secondary select-none">127.0.0.1:6379&gt;</span>
              <span className="text-text-primary">MULTI</span>
              <span className="text-text-secondary select-none">:</span>
              <span className="text-[#00E5A3] font-bold">OK</span>
            </span>

            <span className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-text-secondary select-none">127.0.0.1:6379(TX)&gt;</span>
              <span className="text-text-primary">INCR counter:visits</span>
              <span className="text-text-secondary select-none">:</span>
              <span className="text-[#00E5A3] font-bold">QUEUED</span>
            </span>

            <span className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-text-secondary select-none">127.0.0.1:6379(TX)&gt;</span>
              <span className="text-text-primary">EXEC</span>
              <span className="text-text-secondary select-none">:</span>
              <span className="text-[#FBBF24]">1) (integer) 1</span>
            </span>

            <span className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-text-secondary select-none">127.0.0.1:6379&gt;</span>
              <span className="text-text-primary">DBSIZE</span>
              <span className="text-text-secondary select-none">:</span>
              <span className="text-[#FBBF24]">(integer) 3</span>
            </span>

            <span className="flex items-center gap-2 whitespace-nowrap pt-0.5">
              <span className="text-text-secondary select-none">127.0.0.1:6379&gt;</span>
              <span className="w-2.5 h-4 bg-accent-cyan inline-block animate-pulse" />
            </span>
          </code>
        </div>
      </Reveal>

      <div className="w-full max-w-[960px] grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-2.5 sm:gap-4">
        {metrics.map((metric, idx) => {
          const Icon = metric.icon
          return (
            <Reveal key={metric.title} direction="up" delay={idx * 75} className="h-full">
              <div className="h-full bg-surface-card rounded-xl p-5 sm:p-6 flex flex-col justify-between gap-2.5 sm:gap-3 transition-colors">
                <div className="flex flex-wrap items-center justify-between gap-3">
                  <div className="w-8 h-8 rounded-lg bg-accent-cyan/10 flex items-center justify-center">
                    <Icon className="size-4 text-accent-cyan" />
                  </div>
                  <span className="text-lg sm:text-xl font-bold tabular-nums text-text-primary">{metric.value}</span>
                </div>
                <div>
                  <h4 className="text-[13px] font-semibold text-text-primary mb-0.5">{metric.title}</h4>
                  <p className="text-[13px] text-text-secondary">{metric.comparison}</p>
                </div>
              </div>
            </Reveal>
          )
        })}
      </div>
    </section>
  )
}
