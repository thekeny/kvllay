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
        <div className="w-full bg-[#0B0E17] border border-[#1E3B5C] rounded-xl overflow-hidden shadow-[0_20px_50px_rgba(0,0,0,0.55)]">
          <div className="flex items-center justify-between px-3.5 sm:px-5 py-2 sm:py-3 bg-[#0A0E17] border-b border-[#1B2436]">
            <div className="flex items-center gap-2">
              <span className="w-[10px] sm:w-[11px] h-[10px] sm:h-[11px] rounded-full bg-[#EF4444] inline-block" />
              <span className="w-[10px] sm:w-[11px] h-[10px] sm:h-[11px] rounded-full bg-[#F59E0B] inline-block" />
              <span className="w-[10px] sm:w-[11px] h-[10px] sm:h-[11px] rounded-full bg-[#10B981] inline-block" />
            </div>

            <div className="text-[11px] sm:text-[12px] font-mono text-[#8B9BB4] select-none tracking-wide truncate px-2">
              redis-cli — kvllay v1.0.0 (127.0.0.1:6379)
            </div>

            <div/>
          </div>

          <div className="p-3.5 sm:px-6 sm:py-5 font-mono text-xs sm:text-[13px] leading-[1.75] sm:leading-[1.8] flex flex-col gap-2 sm:gap-2.5 overflow-x-auto">
            <div className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-[#00E5A3] font-bold select-none">user@edge:~$</span>
              <span className="text-[#F0F6FC]">redis-cli -p 6379</span>
            </div>

            <div className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-[#8B9BB4] select-none">127.0.0.1:6379&gt;</span>
              <span className="text-[#F0F6FC] font-semibold">PING</span>
              <span className="text-[#546682] select-none">➔</span>
              <span className="text-[#00E5A3] font-bold">PONG</span>
            </div>

            <div className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-[#8B9BB4] select-none">127.0.0.1:6379&gt;</span>
              <span className="text-[#F0F6FC]">SET user:101 '{'{"name":"Alex","role":"dev"}'}'</span>
              <span className="text-[#546682] select-none">➔</span>
              <span className="text-[#00E5A3] font-bold">OK</span>
            </div>

            <div className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-[#8B9BB4] select-none">127.0.0.1:6379&gt;</span>
              <span className="text-[#F0F6FC]">GET user:101</span>
              <span className="text-[#546682] select-none">➔</span>
              <span className="text-[#38BDF8]">"&#123;\"name\":\"Alex\",\"role\":\"dev\"&#125;"</span>
            </div>

            <div className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-[#8B9BB4] select-none">127.0.0.1:6379&gt;</span>
              <span className="text-[#F0F6FC]">SETEX session:token 3600 "active"</span>
              <span className="text-[#546682] select-none">➔</span>
              <span className="text-[#00E5A3] font-bold">OK</span>
            </div>

            <div className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-[#8B9BB4] select-none">127.0.0.1:6379&gt;</span>
              <span className="text-[#F0F6FC]">TTL session:token</span>
              <span className="text-[#546682] select-none">➔</span>
              <span className="text-[#FBBF24]">(integer) 3600</span>
            </div>

            <div className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-[#8B9BB4] select-none">127.0.0.1:6379&gt;</span>
              <span className="text-[#F0F6FC]">MULTI</span>
              <span className="text-[#546682] select-none">➔</span>
              <span className="text-[#00E5A3] font-bold">OK</span>
            </div>

            <div className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-[#8B9BB4] select-none">127.0.0.1:6379(TX)&gt;</span>
              <span className="text-[#F0F6FC]">INCR counter:visits</span>
              <span className="text-[#546682] select-none">➔</span>
              <span className="text-[#00E5A3] font-bold">QUEUED</span>
            </div>

            <div className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-[#8B9BB4] select-none">127.0.0.1:6379(TX)&gt;</span>
              <span className="text-[#F0F6FC]">EXEC</span>
              <span className="text-[#546682] select-none">➔</span>
              <span className="text-[#FBBF24]">1) (integer) 1</span>
            </div>

            <div className="flex items-center gap-2.5 whitespace-nowrap">
              <span className="text-[#8B9BB4] select-none">127.0.0.1:6379&gt;</span>
              <span className="text-[#F0F6FC]">DBSIZE</span>
              <span className="text-[#546682] select-none">➔</span>
              <span className="text-[#FBBF24]">(integer) 3</span>
            </div>

            <div className="flex items-center gap-2 whitespace-nowrap pt-0.5">
              <span className="text-[#8B9BB4] select-none">127.0.0.1:6379&gt;</span>
              <span className="w-2.5 h-4 bg-[#00D2FF] inline-block animate-pulse" />
            </div>
          </div>
        </div>
      </Reveal>

      <div className="w-full max-w-[960px] grid grid-cols-1 sm:grid-cols-2 md:grid-cols-4 gap-2.5 sm:gap-4">
        {metrics.map((metric, idx) => {
          const Icon = metric.icon
          return (
            <Reveal key={metric.title} direction="up" delay={idx * 75} className="h-full">
              <div className="h-full bg-[#0E131F] border border-[#1B2436] hover:border-[#1E3B5C] rounded-xl p-3.5 sm:p-4.5 flex flex-col justify-between gap-2.5 sm:gap-3 transition-colors shadow-sm">
                <div className="flex items-center justify-between">
                  <div className="w-8 h-8 rounded-lg bg-[#00D2FF]/10 flex items-center justify-center">
                    <Icon className="size-4 text-[#00D2FF]" />
                  </div>
                  <span className="text-lg sm:text-xl font-bold font-mono text-[#F0F6FC]">{metric.value}</span>
                </div>
                <div>
                  <h4 className="text-[12.5px] sm:text-[13px] font-semibold text-[#F0F6FC] mb-0.5">{metric.title}</h4>
                  <p className="text-[11px] sm:text-[11.5px] text-[#8B9BB4]">{metric.comparison}</p>
                </div>
              </div>
            </Reveal>
          )
        })}
      </div>
    </section>
  )
}
