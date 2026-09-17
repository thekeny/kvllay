import { IconBook2, IconExternalLink, IconFileText, IconCircleCheck } from "@tabler/icons-react"
import { links } from "@/config/links"
import { paths } from "@/config/paths"
import { Reveal } from "@/components/ui/Reveal"

export function Documentation() {
  const docsGuides = [
    {
      title: "Multi-Reactor Architecture & Sharding",
      desc: "Detailed explanation of the 32-way sharded storage engine, Epoll/WSAPoll Reactor-per-Thread worker loops, in-place memory buffer reuse, and lock-free dispatching.",
    },
    {
      title: "Redis RESP2 Protocol, Transactions & Handshakes",
      desc: "Complete reference for Strings, Lists, Counters, atomic MULTI/EXEC/DISCARD transactions, HELLO protocol negotiation, and CLIENT connection management.",
    },
    {
      title: "Crash Recovery, Persistence & Benchmarking",
      desc: "Zero-fork point-in-time CRC32 snapshots, double-buffered asynchronous AOF with BGREWRITEAOF, corruption recovery, and official redis-benchmark reproduction.",
    },
  ]

  return (
    <section id={paths.sections.docs} className="w-full py-10 sm:py-16 px-4 sm:px-6 lg:px-10 max-w-[1240px] mx-auto scroll-mt-20">
      <div className="w-full bg-[#0E131F] border border-[#1E3B5C] rounded-2xl sm:rounded-3xl p-5 sm:p-12 lg:p-14 relative overflow-hidden shadow-[0_25px_60px_rgba(0,0,0,0.55)]">
        <div className="absolute top-0 right-0 w-96 h-96 bg-[#00D2FF]/5 rounded-full blur-3xl pointer-events-none" />
        <div className="absolute bottom-0 left-0 w-80 h-80 bg-[#00E5A3]/5 rounded-full blur-3xl pointer-events-none" />

        <div className="relative z-10 grid grid-cols-1 lg:grid-cols-12 gap-6 lg:gap-12 items-center">
          <Reveal direction="left" delay={50} className="lg:col-span-7 flex flex-col gap-4 sm:gap-6">
            <div className="inline-flex items-center gap-2 px-3.5 py-1.5 rounded-full bg-[#00D2FF]/10 border border-[#00D2FF]/30 w-fit">
              <IconBook2 className="size-3.5 text-[#00D2FF]" />
              <span className="text-[11px] font-mono font-bold text-[#00D2FF] tracking-wider uppercase">
                DOCUMENTATION & GUIDES
              </span>
            </div>

            <div className="flex flex-col gap-2.5 sm:gap-3">
              <h2 className="text-2xl sm:text-3xl lg:text-4xl font-bold text-[#F0F6FC] tracking-tight leading-tight">
                Deep Dive into the Architecture & Protocol
              </h2>
              <p className="text-sm sm:text-base text-[#8B9BB4] leading-[1.6]">
                Kvllay comes with comprehensive technical documentation in both Russian and English, covering internal memory management, thread-safe command dispatching, and full benchmark reproducibility.
              </p>
            </div>

            <div className="flex flex-col gap-2.5 sm:gap-3 pt-1">
              {docsGuides.map((guide, idx) => (
                <div key={idx} className="flex items-start gap-2.5 sm:gap-3">
                  <div className="w-5 h-5 rounded-full bg-[#00D2FF]/15 flex items-center justify-center shrink-0 mt-0.5">
                    <IconCircleCheck className="size-3.5 text-[#00D2FF]" />
                  </div>
                  <div>
                    <h4 className="text-[13.5px] sm:text-[14px] font-semibold text-[#F0F6FC]">{guide.title}</h4>
                    <p className="text-[12px] sm:text-[12.5px] text-[#8B9BB4] leading-relaxed">{guide.desc}</p>
                  </div>
                </div>
              ))}
            </div>

            <div className="flex flex-wrap items-center gap-2.5 sm:gap-3.5 pt-2 sm:pt-3">
              <a
                href={links.docsEn}
                target="_blank"
                rel="noreferrer"
                className="flex items-center justify-center gap-2 px-4.5 sm:px-5.5 py-2.5 sm:py-3 bg-[#00D2FF] hover:bg-[#00b8e6] text-[#08090E] rounded-xl text-[13px] sm:text-[14px] font-bold transition-all shadow-[0_0_20px_rgba(0,210,255,0.2)] hover:shadow-[0_0_25px_rgba(0,210,255,0.35)] w-full sm:w-auto"
              >
                <IconFileText className="size-4 stroke-[2.5]" strokeWidth={2.5} />
                <span>English Docs (en.md)</span>
                <IconExternalLink className="size-3.5 opacity-80" />
              </a>

              <a
                href={links.docsRu}
                target="_blank"
                rel="noreferrer"
                className="flex items-center justify-center gap-2 px-4.5 sm:px-5 py-2.5 sm:py-3 bg-[#0A0E17] hover:bg-[#161D2E] border border-[#1E3B5C] hover:border-[#00D2FF]/50 text-[#F0F6FC] rounded-xl text-[13px] sm:text-[14px] font-medium transition-colors w-full sm:w-auto"
              >
                <IconFileText className="size-4 text-[#8B9BB4]" />
                <span>Russian Docs (ru.md)</span>
                <IconExternalLink className="size-3.5 text-[#546682]" />
              </a>
            </div>
          </Reveal>

          <Reveal direction="right" delay={120} className="lg:col-span-5 flex justify-center items-center">
            <div className="relative group max-w-[320px] sm:max-w-[360px] w-full">
              <div className="absolute -inset-2 bg-gradient-to-tr from-[#00D2FF]/20 via-[#38BDF8]/10 to-[#00E5A3]/20 rounded-3xl blur-xl opacity-75 group-hover:opacity-100 transition-opacity" />

              <div className="relative bg-[#070A10] border border-[#1E3B5C] rounded-2xl p-4 sm:p-5 flex flex-col items-center gap-3 overflow-hidden shadow-2xl">
                <div className="w-full flex items-center justify-between pb-2 border-b border-[#1B2436] text-xs font-mono text-[#8B9BB4]">
                  <div className="flex items-center gap-2">
                    <span className="w-2.5 h-2.5 rounded-full bg-[#EF4444]" />
                    <span className="w-2.5 h-2.5 rounded-full bg-[#F59E0B]" />
                    <span className="w-2.5 h-2.5 rounded-full bg-[#10B981]" />
                  </div>
                  <span className="text-[11px] text-[#546682]">mind_allay.gif</span>
                </div>

                <div className="w-full rounded-xl overflow-hidden bg-[#0A0E17] flex items-center justify-center">
                  <picture className="w-full flex items-center justify-center">
                    <source srcSet={paths.assets.allayDocsWebp} type="image/webp" />
                    <img
                      src={paths.assets.allayDocsGif}
                      alt="Kvllay Allay Mascot Reading Docs"
                      className="w-full h-auto max-h-[380px] object-contain select-none"
                    />
                  </picture>
                </div>

                <div className="w-full text-center pt-1">
                  <p className="text-xs font-mono text-[#00D2FF]">
                    {"You sure you don't want to try kvllay? >:3"}
                  </p>
                </div>
              </div>
            </div>
          </Reveal>
        </div>
      </div>
    </section>
  )
}
