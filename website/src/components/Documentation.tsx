import { IconExternalLink, IconFileText, IconCircleCheck } from "@tabler/icons-react"
import { links } from "@/config/links"
import { paths } from "@/config/paths"
import { images } from "@/config/images"
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
    <section id={paths.sections.docs} className="w-full py-8 sm:py-12 px-4 sm:px-6 lg:px-10 max-w-[1240px] mx-auto scroll-mt-20">
      <div className="w-full bg-surface-card rounded-2xl sm:rounded-3xl p-5 sm:p-12 lg:p-14 relative overflow-hidden">

        <div className="relative z-10 grid grid-cols-1 lg:grid-cols-12 gap-6 lg:gap-12 items-center">
          <Reveal direction="left" delay={50} className="lg:col-span-7 flex flex-col gap-4 sm:gap-6">
<div className="flex flex-col gap-2.5 sm:gap-3">
              <h2 className="text-2xl sm:text-3xl lg:text-4xl font-bold text-text-primary tracking-tight leading-tight">
                Deep Dive into the Architecture & Protocol
              </h2>
              <p className="text-base text-text-secondary leading-[1.6]">
                Kvllay comes with comprehensive technical documentation in both Russian and English, covering internal memory management, thread-safe command dispatching, and full benchmark reproducibility.
              </p>
            </div>

            <div className="flex flex-col gap-2.5 sm:gap-3 pt-1">
              {docsGuides.map((guide, idx) => (
                <div key={idx} className="flex items-start gap-2.5 sm:gap-3">
                  <div className="w-5 h-5 rounded-full bg-accent-cyan/15 flex items-center justify-center shrink-0 mt-0.5">
                    <IconCircleCheck className="size-3.5 text-accent-cyan" />
                  </div>
                  <div>
                    <h4 className="text-[13.5px] sm:text-[14px] font-semibold text-text-primary">{guide.title}</h4>
                    <p className="text-base text-text-secondary leading-relaxed">{guide.desc}</p>
                  </div>
                </div>
              ))}
            </div>

            <div className="flex flex-wrap items-center gap-2.5 sm:gap-3.5 pt-2 sm:pt-3">
              <a
                href={links.docsEn}
                target="_blank"
                rel="noreferrer"
                className="flex items-center justify-center gap-2 px-4.5 sm:px-5.5 py-2.5 sm:py-3 bg-accent-cyan hover:bg-[#00b8e6] text-primary-foreground rounded-xl text-[13px] sm:text-[14px] font-bold transition-colors w-full sm:w-auto"
              >
                <IconFileText className="size-4 stroke-[2.5]" strokeWidth={2.5} />
                <span>English Docs (en.md)</span>
                <IconExternalLink className="size-3.5 opacity-80" />
              </a>

              <a
                href={links.docsRu}
                target="_blank"
                rel="noreferrer"
                className="flex items-center justify-center gap-2 px-4.5 sm:px-5 py-2.5 sm:py-3 bg-surface-subtle hover:bg-[#161D2E] text-text-primary rounded-xl text-[13px] sm:text-[14px] font-medium transition-colors w-full sm:w-auto"
              >
                <IconFileText className="size-4 text-text-secondary" />
                <span>Russian Docs (ru.md)</span>
                <IconExternalLink className="size-3.5 text-text-secondary" />
              </a>
            </div>
          </Reveal>

          <Reveal direction="right" delay={120} className="lg:col-span-5 flex justify-center items-center">
            <div className="relative group max-w-[320px] sm:max-w-[360px] w-full">

              <div className="relative bg-canvas rounded-2xl p-4 sm:p-5 flex flex-col items-center gap-3 overflow-hidden">
                <div className="w-full flex items-center justify-between pb-2 text-[13px] tabular-nums text-text-secondary">
                  <div className="flex items-center gap-2">
                    <span className="w-2.5 h-2.5 rounded-full bg-[#EF4444]" />
                    <span className="w-2.5 h-2.5 rounded-full bg-[#F59E0B]" />
                    <span className="w-2.5 h-2.5 rounded-full bg-[#10B981]" />
                  </div>
                  <span className="text-[13px] text-text-secondary">mind_allay.gif</span>
                </div>

                <div className="w-full rounded-xl overflow-hidden bg-surface-subtle flex items-center justify-center">
                  <picture className="w-full flex items-center justify-center">
                    <source srcSet={images.allayDocsWebp} type="image/webp" />
                    <img
                      src={images.allayDocsGif}
                      alt="Kvllay Allay Mascot Reading Docs"
                      className="w-full h-auto max-h-[380px] object-contain select-none"
                    />
                  </picture>
                </div>

                <div className="w-full text-center pt-1">
                  <p className="text-[13px] tabular-nums text-accent-cyan">
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
