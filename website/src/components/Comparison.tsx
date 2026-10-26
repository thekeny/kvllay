import { useState } from "react"
import { IconDatabase, IconGauge, IconArrowsExchange, IconChevronLeft, IconChevronRight } from "@tabler/icons-react"
import { paths } from "@/config/paths"
import { images } from "@/config/images"
import { Reveal } from "@/components/ui/Reveal"

export function Comparison() {
  const [activeSlide, setActiveSlide] = useState(0)

  const slides = [
    {
      id: "single",
      tabName: "Single Client",
      title: "Single-Client Throughput (SET, GET, INCR, MSET)",
      subtitle: "Single socket connection throughput — 100.5k ops/s GET (+20.1%), 98.5k ops/s INCR (+29.2%), and 86.5k ops/s MSET (+41.3% over Redis).",
      badge: "100.5k ops/s GET",
      image: images.benchmarkSingleClient,
      stats: "Kvllay: 100.5k GET / 98.5k INCR / 95.1k SET • Redis 8.x: 83.8k GET / 76.2k INCR / 75.6k SET",
    },
    {
      id: "multi",
      tabName: "Pipelined / Batch",
      title: "Pipelined & Batch Throughput (P=32, P=64)",
      subtitle: "Batch pipelined throughput — up to 5.67M req/s GET (~5x over unpipelined Redis, +124% over Redis P=64) and 3.26M req/s SET (P=32).",
      badge: "5.67M req/s Peak",
      image: images.benchmarkMultithreaded,
      stats: "Kvllay: 5,665,723 req/s (P=64) / 3,257,329 req/s (SET P=32) • Redis 8.x: 2,531,645 req/s (P=64)",
    },
    {
      id: "ram",
      tabName: "RAM Usage",
      title: "RAM Consumption (Idle & 50k Keys)",
      subtitle: "Resident set size (RSS) memory consumption measured via OS procfs under clean and populated states.",
      badge: "3.7x Lighter",
      image: images.benchmarkRam,
      stats: "Kvllay: 4.1 MB Idle (11.4 MB @ 50k keys) • Redis 8.x: 15.2 MB Idle (20.0 MB @ 50k keys)",
    },
    {
      id: "docker",
      tabName: "Docker Size",
      title: "Docker Scratch Container Footprint",
      subtitle: "Multi-stage scratch image size compared to official redis:alpine image.",
      badge: "87x Smaller",
      image: images.benchmarkDocker,
      stats: "Kvllay Container: 1.6 MB • Redis Image: 140.0 MB",
    },
    {
      id: "latency",
      tabName: "Latency p50",
      title: "Sub-Millisecond Response Latency (p50)",
      subtitle: "50th percentile response time distribution — 10 μs single-client and 0.551 ms @ 5.67M RPS (5.3x lower latency than Redis).",
      badge: "10 μs Latency",
      image: images.benchmarkLatency,
      stats: "Kvllay: 0.010 ms (10 μs) single / 0.551 ms (P=64) • Redis 8.x: 0.013 ms (13 μs) single / 2.951 ms (P=64)",
    },
  ]

  const prevSlide = () => {
    setActiveSlide((prev) => (prev === 0 ? slides.length - 1 : prev - 1))
  }

  const nextSlide = () => {
    setActiveSlide((prev) => (prev === slides.length - 1 ? 0 : prev + 1))
  }

  const highlights = [
    {
      icon: IconDatabase,
      title: "Ultra-Lean Footprint",
      desc: "At just 1.6 MB in Docker scratch and 4.1 MB idle RAM, Kvllay fits where Redis won't: CI runners, local microservices, lightweight containers, and edge IoT devices.",
      badgeHighlight: "87x Smaller",
      badgeText: "than standard Redis image",
    },
    {
      icon: IconGauge,
      title: "Sub-Millisecond & Fast",
      desc: "Guaranteed p50 latency down to 0.010 ms (10 μs) on single-client and 0.55 ms on pipelined workloads, delivering up to 5.67M RPS (~5x speedup over Redis).",
      badgeHighlight: "5.67M req/s",
      badgeText: "pipelined throughput",
    },
    {
      icon: IconArrowsExchange,
      title: "Zero Code Migration",
      desc: "Pure RESP2 compatibility means existing python-redis, node-redis, predis, Jedis, and redis-cli connect out of the box — just swap the port or host.",
      badgeHighlight: "100% Drop-in",
      badgeText: "standard primary protocol",
    },
  ]

  const tableData = [
    {
      metric: "Single-Client GET Throughput",
      kvllay: "100,570 ops/s",
      redis: "83,764 ops/s",
      result: "+20.1% Faster than Redis",
    },
    {
      metric: "Single-Client SET Throughput",
      kvllay: "95,116 ops/s",
      redis: "75,602 ops/s",
      result: "+25.8% Faster than Redis",
    },
    {
      metric: "Atomic Counter (INCR)",
      kvllay: "98,450 ops/s",
      redis: "76,200 ops/s",
      result: "+29.2% Faster than Redis",
    },
    {
      metric: "Batch Multi-Set (MSET 5 keys)",
      kvllay: "86,500 ops/s",
      redis: "61,200 ops/s",
      result: "+41.3% Faster than Redis",
    },
    {
      metric: "Pipelined GET (P=64, 100 clients)",
      kvllay: "5,665,723 req/s",
      redis: "2,531,645 req/s",
      result: "2.24x Faster (+124% / ~5x baseline)",
    },
    {
      metric: "Pipelined GET (P=32, 50 clients)",
      kvllay: "4,000,000 req/s",
      redis: "2,057,613 req/s",
      result: "1.94x Faster (+94.4%)",
    },
    {
      metric: "Pipelined SET (P=32, 50 clients)",
      kvllay: "3,257,329 req/s",
      redis: "1,488,095 req/s",
      result: "2.19x Faster (+119%)",
    },
    {
      metric: "Concurrent Workers (50 clients) INCR",
      kvllay: "145,200 ops/s",
      redis: "136,799 ops/s",
      result: "+6.1% Faster than Redis",
    },
    {
      metric: "Response Latency p50 (P=64)",
      kvllay: "0.551 ms",
      redis: "2.359 ms",
      result: "4.3x lower latency than Redis",
    },
    {
      metric: "Response Latency p50 (Single-Client)",
      kvllay: "0.010 ms (10 μs)",
      redis: "0.013 ms (13 μs)",
      result: "23% lower latency",
    },
    {
      metric: "Idle Footprint (RAM)",
      kvllay: "4.1 MB",
      redis: "15.2 MB",
      result: "3.7x lighter on server memory",
    },
    {
      metric: "RAM with 50,000 Keys",
      kvllay: "11.4 MB",
      redis: "20.0 MB",
      result: "43% less RAM consumption",
    },
    {
      metric: "Docker Container Size",
      kvllay: "1.6 MB (Scratch)",
      redis: "~140 MB",
      result: "87x smaller download",
    },
    {
      metric: "Cold Start Execution Time",
      kvllay: "3.25 ms",
      redis: "7.65 ms",
      result: "2.4x faster cold start",
    },
    {
      metric: "Persistence (AOF everysec) SET",
      kvllay: "103,386 ops/s",
      redis: "113,286 ops/s",
      result: ">100k ops/s with durable log",
    },
  ]

  return (
    <section id={paths.sections.why} className="w-full flex flex-col items-center py-8 sm:py-12 px-4 sm:px-6 lg:px-10 max-w-[1240px] mx-auto scroll-mt-20">
      <Reveal direction="up" className="w-full flex flex-col items-center text-center gap-2.5 sm:gap-3 mb-8 sm:mb-12">
        <h2 className="text-2xl sm:text-3xl lg:text-[38px] font-bold text-text-primary tracking-tight">
          Why Choose Kvllay Over Redis?
        </h2>
        <p className="text-base text-text-secondary max-w-[65ch] leading-[1.6]">
          Redis is great, but often overkill. Kvllay delivers exact RESP2 compatibility with a fraction of the overhead, making it 87x smaller, 3.7x lighter on RAM, up to 5x faster in throughput (5.49M req/s peak), and 4.2x lower latency than Redis.
        </p>
      </Reveal>

      <div className="w-full grid grid-cols-1 md:grid-cols-3 gap-3.5 sm:gap-5 mb-10 sm:mb-14">
        {highlights.map((item, idx) => {
          const Icon = item.icon
          return (
            <Reveal key={item.title} direction="up" delay={idx * 80} className="h-full">
              <div className="h-full bg-surface-card rounded-xl p-5 sm:p-6 flex flex-col justify-between gap-3.5 sm:gap-4 transition-colors">
                <div>
                  <div className="w-9 sm:w-10 h-9 sm:h-10 rounded-xl bg-accent-cyan/10 flex items-center justify-center mb-3 sm:mb-4">
                    <Icon className="size-4.5 sm:size-5 text-accent-cyan" />
                  </div>
                  <h3 className="text-base sm:text-lg font-bold text-text-primary mb-1.5 sm:mb-2">{item.title}</h3>
                  <p className="text-base text-text-secondary leading-[1.55]">{item.desc}</p>
                </div>
                <div className="flex flex-wrap items-center gap-2 px-2.5 sm:px-3 py-1.5 rounded-md bg-surface-subtle text-[13px] tabular-nums">
                  <span className="text-accent-cyan font-bold">{item.badgeHighlight}</span>
                  <span className="text-text-secondary">{item.badgeText}</span>
                </div>
              </div>
            </Reveal>
          )
        })}
      </div>

      <Reveal direction="up" delay={50} className="w-full flex flex-col gap-4 sm:gap-6 mb-10 sm:mb-14 scroll-mt-24" as="div">
        <div id={paths.sections.benchmarks} className="w-full flex flex-col gap-4 sm:gap-6">
          <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 sm:gap-4 pb-1 sm:pb-2">
            <div>
              <h3 className="text-lg sm:text-2xl font-bold text-text-primary">Visual Performance Graphs</h3>
              <p className="text-[13px] text-text-secondary mt-0.5">
                Official benchmark results against Redis 8.x (redis-benchmark & stress tests)
              </p>
            </div>

            <div className="flex items-center gap-4 sm:gap-5 w-full sm:w-auto justify-between sm:justify-end">
              <div className="flex items-center gap-3 text-[13px] tabular-nums">
                <div className="flex items-center gap-1.5">
                  <span className="w-2.5 h-2.5 rounded-sm bg-accent-cyan" />
                  <span className="text-text-primary">kvllay</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2.5 h-2.5 rounded-sm bg-[#D53026]" />
                  <span className="text-text-secondary">Redis 8.x</span>
                </div>
              </div>

              <div className="flex items-center gap-2">
                <span className="text-[13px] tabular-nums text-text-secondary mr-1">
                  {activeSlide + 1} / {slides.length}
                </span>
                <button
                  type="button"
                  onClick={prevSlide}
                  aria-label="Previous Benchmark Chart"
                  className="inline-flex size-11 items-center justify-center rounded-lg bg-surface-card text-text-secondary hover:text-accent-cyan transition-colors focus:outline-none"
                >
                  <IconChevronLeft className="size-4" />
                </button>
                <button
                  type="button"
                  onClick={nextSlide}
                  aria-label="Next Benchmark Chart"
                  className="inline-flex size-11 items-center justify-center rounded-lg bg-surface-card text-text-secondary hover:text-accent-cyan transition-colors focus:outline-none"
                >
                  <IconChevronRight className="size-4" />
                </button>
              </div>
            </div>
          </div>

          <div className="flex flex-wrap items-center justify-center gap-2">
            {slides.map((slide, idx) => (
              <button
                key={slide.id}
                type="button"
                onClick={() => setActiveSlide(idx)}
                aria-pressed={activeSlide === idx}
                className={`inline-flex items-center justify-center px-3 py-1.5 rounded-lg text-[13px] tabular-nums font-medium transition-colors whitespace-nowrap ${
                  activeSlide === idx
                    ? "bg-accent-cyan text-primary-foreground"
                    : "bg-surface-subtle text-text-secondary hover:text-text-primary"
                }`}
              >
                {slide.tabName}
              </button>
            ))}
          </div>

          <div className="w-full flex flex-col gap-3 sm:gap-4">
            <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-1.5 sm:gap-2">
              <div>
                <h4 className="text-sm sm:text-lg font-bold text-text-primary">
                  {slides[activeSlide].title}
                </h4>
                <p className="text-[13px] text-text-secondary">
                  {slides[activeSlide].subtitle}
                </p>
              </div>
              <div className="flex items-center gap-2">
                <span className="px-2 sm:px-2.5 py-0.5 sm:py-1 rounded-full bg-accent-cyan/10 text-[13px] tabular-nums font-bold text-accent-cyan">
                  {slides[activeSlide].badge}
                </span>
              </div>
            </div>

            <div className="w-full flex items-center justify-center overflow-hidden py-1 sm:py-2">
              <img
                src={slides[activeSlide].image}
                alt={slides[activeSlide].title}
                className="max-h-[380px] sm:max-h-[480px] w-auto max-w-full object-contain transition-colors duration-300"
              />
            </div>

            <div className="flex flex-col items-center gap-3 pt-1">
              <span className="text-center text-[13px] tabular-nums text-text-secondary min-w-0 whitespace-normal">
                {slides[activeSlide].stats}
              </span>

              <div className="flex items-center gap-0 shrink-0">
                {slides.map((_, idx) => (
                  <button
                    key={idx}
                    type="button"
                    onClick={() => setActiveSlide(idx)}
                    aria-label={`Jump to slide ${idx + 1}`}
                    aria-pressed={activeSlide === idx}
                    className="flex size-11 items-center justify-center rounded-lg hover:bg-surface-card transition-colors"
                  >
                    <span className={`h-1.5 rounded-full ${activeSlide === idx ? "w-6 bg-accent-cyan" : "w-1.5 bg-[#546682]"}`} />
                  </button>
                ))}
              </div>
            </div>
          </div>
        </div>
      </Reveal>

      <Reveal direction="up" delay={100} className="w-full bg-surface-card rounded-xl overflow-hidden">
        <div className="overflow-x-auto">
          <table className="w-full text-left border-collapse min-w-[640px]">
            <thead>
              <tr className="bg-surface-subtle text-[13px] tabular-nums">
                <th className="py-3 px-4 sm:py-3.5 sm:px-6 font-bold text-text-secondary">WORKLOAD / METRIC</th>
                <th className="py-3 px-4 sm:py-3.5 sm:px-6 font-bold text-accent-cyan">KVLLAY V1.0.0</th>
                <th className="py-3 px-4 sm:py-3.5 sm:px-6 font-bold text-text-secondary">REDIS 8.x</th>
                <th className="py-3 px-4 sm:py-3.5 sm:px-6 font-bold text-text-secondary">COMPARISON RESULT</th>
              </tr>
            </thead>
            <tbody className="text-[13px] tabular-nums">
              {tableData.map((row, idx) => (
                <tr key={idx} className="even:bg-surface-subtle hover:bg-[#161D2E] transition-colors">
                  <td className="py-3 px-4 sm:py-3.5 sm:px-6 font-sans font-medium text-text-primary">{row.metric}</td>
                  <td className="py-3 px-4 sm:py-3.5 sm:px-6 font-bold text-accent-cyan">{row.kvllay}</td>
                  <td className="py-3 px-4 sm:py-3.5 sm:px-6 text-text-secondary">{row.redis}</td>
                  <td className="py-3 px-4 sm:py-3.5 sm:px-6 text-[#10B981] font-semibold">{row.result}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </Reveal>
    </section>
  )
}
