import { IconTerminal2, IconShield, IconClock, IconBox, IconArrowsExchange, IconCpu } from "@tabler/icons-react"
import { paths } from "@/config/paths"
import { Reveal } from "@/components/ui/Reveal"

export function FeaturesBento() {
  const features = [
    {
      icon: IconTerminal2,
      title: "100% RESP2 & Protocol Handshake",
      desc: "Implements Redis RESP2 serialization, HELLO 2/3 negotiation, CLIENT management (ID, SETNAME, LIST), and SELECT 0 so official SDKs connect out of the box.",
    },
    {
      icon: IconShield,
      title: "Multi-Reactor & Sharding",
      desc: "Non-blocking Epoll & WSAPoll worker pool combined with 32-way cache-line aligned (alignas(64)) lock-striped storage. Delivers over 5.67M req/s with zero false sharing.",
    },
    {
      icon: IconArrowsExchange,
      title: "ACID Transactions & Pipelines",
      desc: "Full MULTI, EXEC, and DISCARD support with strict connection-local transactional queuing and zero-delay non-blocking pipelining.",
    },
    {
      icon: IconClock,
      title: "Zero-Fork Snapshots & AOF",
      desc: "Point-in-time CRC32 snapshots without Redis fork() latency or COW RAM doubling. Double-buffered AOF with BGREWRITEAOF, TTL sweeps, and LRU eviction.",
    },
    {
      icon: IconCpu,
      title: "Memory Compaction & Allocators",
      desc: "In-place string buffer reuse eliminating heap fragmentation, zero-allocation counters, and pluggable allocators (libc, jemalloc, mimalloc) with RSS tracking.",
    },
    {
      icon: IconBox,
      title: "1.6 MB Scratch & Signal Safety",
      desc: "Ultra-lean 1.6 MB Docker scratch build with zero glibc dependencies. Full OS signal handling (SIGINT/SIGTERM) guarantees clean persistence flushing on shutdown.",
    },
  ]

  return (
    <section id={paths.sections.features} className="w-full flex flex-col items-center py-8 sm:py-12 px-4 sm:px-6 lg:px-10 max-w-[1240px] mx-auto scroll-mt-20">
      <Reveal direction="up" className="flex flex-col items-center text-center gap-2.5 sm:gap-3 mb-8 sm:mb-12">
        <h2 className="text-2xl sm:text-3xl lg:text-[38px] font-bold text-text-primary tracking-tight">
          Built for Speed, Simplicity &amp; Reliability
        </h2>
        <p className="text-base text-text-secondary max-w-[65ch] leading-[1.6]">
          Every subsystem is hand-crafted in modern C++17 to eliminate bloat, guarantee safety, and deliver predictable ultra-low latency.
        </p>
      </Reveal>

      <div className="w-full grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3.5 sm:gap-5">
        {features.map((item, idx) => {
          const Icon = item.icon
          return (
            <Reveal key={item.title} direction="up" delay={idx * 60} className="h-full">
              <div className="h-full bg-surface-card rounded-xl p-5 sm:p-6 flex flex-col justify-start gap-2.5 sm:gap-3.5 transition-colors group">
                <div className="w-9 h-9 rounded-lg bg-surface-subtle flex items-center justify-center transition-colors">
                  <Icon className="size-4.5 text-accent-cyan" />
                </div>
                <h3 className="text-[16px] sm:text-[17px] font-bold text-text-primary">{item.title}</h3>
                <p className="text-base text-text-secondary leading-[1.55]">{item.desc}</p>
              </div>
            </Reveal>
          )
        })}
      </div>
    </section>
  )
}
