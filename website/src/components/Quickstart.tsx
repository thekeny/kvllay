import { useState } from "react"
import { IconTerminal2, IconDownload, IconHammer, IconCopy, IconCheck } from "@tabler/icons-react"
import { links } from "@/config/links"
import { paths } from "@/config/paths"
import { Reveal } from "@/components/ui/Reveal"

export function Quickstart() {
  const [copiedCard, setCopiedCard] = useState<number | null>(null)

  const handleCopy = (text: string, index: number) => {
    navigator.clipboard.writeText(text)
    setCopiedCard(index)
    setTimeout(() => setCopiedCard(null), 2000)
  }

  const dockerCode = `docker run -d --name kvllay \\
  -p 6379:6379 kenyka/kvllay:latest`

  const binaryCode = `./kvllay-linux-x86_64 -p 6379
.\\kvllay-windows-x86_64.exe`

  const sourceCode = `git clone github.com/thekeny/kvllay
cd kvllay && make compile
./build/kvllay -p 6379`

  return (
    <section id={paths.sections.quickstart} className="w-full flex flex-col items-center py-8 sm:py-12 px-4 sm:px-6 lg:px-10 max-w-[1240px] mx-auto scroll-mt-20">
      <Reveal direction="up" className="flex flex-col items-center text-center gap-2.5 sm:gap-3 mb-8 sm:mb-12">
        <h2 className="text-2xl sm:text-3xl lg:text-[38px] font-bold text-text-primary tracking-tight">
          Ready in Seconds. Pick Your Workflow.
        </h2>
        <p className="text-base text-text-secondary max-w-[65ch] leading-[1.6]">
          From zero-config Docker scratch images to native standalone executables and single-command local compilation.
        </p>
      </Reveal>

      <div className="w-full grid grid-cols-1 lg:grid-cols-3 gap-4 sm:gap-5 items-stretch">
        <Reveal direction="up" delay={0} className="h-full min-w-0">
          <div className="h-full min-w-0 bg-surface-card rounded-xl p-5 sm:p-6 flex flex-col justify-between">
            <div className="flex flex-col gap-3 sm:gap-3.5 mb-4 sm:mb-5">
              <div className="flex items-center justify-between">
                <div className="w-8.5 sm:w-9 h-8.5 sm:h-9 rounded-lg bg-accent-cyan/15 flex items-center justify-center">
                  <IconTerminal2 className="size-4.5 sm:size-5 text-accent-cyan" />
                </div>
                <span className="px-2.5 py-1 rounded-md bg-accent-cyan text-primary-foreground text-[13px] tabular-nums font-bold">
                  Recommended
                </span>
              </div>
              <h3 className="text-base sm:text-lg font-bold text-text-primary">Docker Container</h3>
              <p className="text-base text-text-secondary leading-[1.55]">
                Ultra-lean 1.6 MB scratch image. Starts up instantly with zero host dependencies.
              </p>
            </div>

            <div className="relative bg-surface-subtle min-w-0 rounded-lg p-3 sm:p-3.5 font-mono text-[13px] text-text-primary flex flex-col gap-1.5 group overflow-x-auto">
              <code className="block text-text-primary whitespace-pre-wrap font-medium leading-relaxed pr-10">
                docker run -d --name kvllay \<br />
                <span className="text-accent-cyan">  -p 6379:6379 kenyka/kvllay:latest</span>
              </code>
              <button
                type="button"
                onClick={() => handleCopy(dockerCode, 1)}
                className="absolute top-1 right-1 p-1.5 text-text-secondary hover:text-accent-cyan transition-colors focus:outline-none"
                title="Copy code"
              >
                {copiedCard === 1 ? <IconCheck className="size-4 text-accent-cyan" /> : <IconCopy className="size-4" />}
              </button>
            </div>
        </div>
        </Reveal>

        <Reveal direction="up" delay={100} className="h-full min-w-0">
          <div className="h-full min-w-0 bg-surface-card rounded-xl p-5 sm:p-6 flex flex-col justify-between transition-colors">
            <div className="flex flex-col gap-3 sm:gap-3.5 mb-4 sm:mb-5">
              <div className="flex items-center justify-between">
                <div className="w-8.5 sm:w-9 h-8.5 sm:h-9 rounded-lg bg-surface-subtle flex items-center justify-center">
                  <IconDownload className="size-4.5 sm:size-5 text-text-primary" />
                </div>
                <span className="px-2 py-0.5 rounded-md bg-surface-subtle text-text-secondary text-[13px] tabular-nums font-medium">
                  Portable
                </span>
              </div>
              <h3 className="text-base sm:text-lg font-bold text-text-primary">Standalone Binaries</h3>
              <p className="text-base text-text-secondary leading-[1.55]">
                Pre-compiled single executables for Linux x86_64 and Windows x86_64 with zero shared libraries.
              </p>
            </div>

            <div className="flex flex-col gap-2.5 sm:gap-3">
              <a
                href={links.releases}
                target="_blank"
                rel="noreferrer"
                className="w-full flex items-center justify-center gap-2 py-2 sm:py-2.5 px-3.5 sm:px-4 bg-accent-cyan hover:bg-[#00b8e6] text-primary-foreground rounded-lg text-[13px] font-bold transition-colors"
              >
                <IconDownload className="size-4 stroke-[2.5]" strokeWidth={2.5} />
                <span>Download Binaries (x86_64)</span>
              </a>

              <div className="relative bg-surface-subtle min-w-0 rounded-lg p-2.5 sm:p-3 font-mono text-[13px] text-text-primary flex flex-col gap-1 group overflow-x-auto">
                <code className="block whitespace-pre text-text-primary pr-10">./kvllay-linux-x86_64 -p 6379</code>
                <code className="block whitespace-pre text-text-primary pr-10">.\kvllay-windows-x86_64.exe</code>
                <button
                  type="button"
                  onClick={() => handleCopy(binaryCode, 2)}
                  className="absolute top-1 right-1 p-1.5 text-text-secondary hover:text-accent-cyan transition-colors focus:outline-none"
                  title="Copy commands"
                >
                  {copiedCard === 2 ? <IconCheck className="size-4 text-accent-cyan" /> : <IconCopy className="size-4" />}
                </button>
              </div>
            </div>
          </div>
        </Reveal>

        <Reveal direction="up" delay={200} className="h-full min-w-0">
          <div className="h-full min-w-0 bg-surface-card rounded-xl p-5 sm:p-6 flex flex-col justify-between transition-colors">
            <div className="flex flex-col gap-3 sm:gap-3.5 mb-4 sm:mb-5">
              <div className="flex items-center justify-between">
                <div className="w-8.5 sm:w-9 h-8.5 sm:h-9 rounded-lg bg-surface-subtle flex items-center justify-center">
                  <IconHammer className="size-4.5 sm:size-5 text-text-primary" />
                </div>
                <span className="px-2 py-0.5 rounded-md bg-surface-subtle text-text-secondary text-[13px] tabular-nums font-medium">
                  C++17 source
                </span>
              </div>
              <h3 className="text-base sm:text-lg font-bold text-text-primary">Build from Source</h3>
              <p className="text-base text-text-secondary leading-[1.55]">
                Compile in seconds with standard g++ or clang++. No external packages or third-party libraries.
              </p>
            </div>

            <div className="relative bg-surface-subtle min-w-0 rounded-lg p-3 sm:p-3.5 font-mono text-[13px] text-text-primary flex flex-col gap-1.5 group overflow-x-auto">
              <code className="block whitespace-pre text-text-primary pr-10">git clone github.com/thekeny/kvllay</code>
              <code className="block whitespace-pre text-text-primary">cd kvllay &amp;&amp; make compile</code>
              <code className="block whitespace-pre text-accent-cyan font-medium">./build/kvllay -p 6379</code>
              <button
                type="button"
                onClick={() => handleCopy(sourceCode, 3)}
                className="absolute top-1 right-1 p-1.5 text-text-secondary hover:text-accent-cyan transition-colors focus:outline-none"
                title="Copy commands"
              >
                {copiedCard === 3 ? <IconCheck className="size-4 text-accent-cyan" /> : <IconCopy className="size-4" />}
              </button>
            </div>
          </div>
        </Reveal>
      </div>
    </section>
  )
}
