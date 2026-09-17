import { useState } from "react"
import { IconArrowRight, IconStar, IconDownload, IconCopy, IconCheck } from "@tabler/icons-react"
import { useGithubStars, formatStars } from "@/lib/useGithubStars"
import { links } from "@/config/links"
import { paths } from "@/config/paths"
import { Reveal } from "@/components/ui/Reveal"

export function Hero() {
  const [copied, setCopied] = useState(false)
  const { stars, isLoading } = useGithubStars()
  const dockerCmd = "docker run -d -p 6379:6379 --name kvllay kenyka/kvllay:latest"

  const handleCopy = () => {
    navigator.clipboard.writeText(dockerCmd)
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  return (
    <section className="w-full flex flex-col items-center pt-6 pb-10 sm:pt-10 sm:pb-12 px-4 sm:px-6 lg:px-10 max-w-[1240px] mx-auto text-center">
      <Reveal direction="down" delay={0}>
        <div className="inline-flex items-center gap-2 px-3 py-1 sm:px-3.5 sm:py-1.5 rounded-full bg-[#0E131F] border border-[#1E3B5C] mb-6 sm:mb-8 shadow-sm max-w-full overflow-x-auto scrollbar-none whitespace-nowrap flex-nowrap">
          <span className="w-2 h-2 rounded-full bg-[#00D2FF] animate-pulse shrink-0" />
          <span className="text-[11px] sm:text-xs font-mono text-[#00D2FF] font-semibold tracking-wide uppercase shrink-0">
            KVLLAY V1.0.0 IS LIVE
          </span>
          <span className="text-[#546682] text-xs shrink-0">•</span>
          <span className="text-[11px] sm:text-xs text-[#8B9BB4] shrink-0">Pure C++ In-Memory Engine</span>
        </div>
      </Reveal>

      <Reveal direction="scale" delay={80}>
        <div className="relative w-28 sm:w-44 lg:w-48 h-auto mb-4 sm:mb-6 drop-shadow-[0_10px_35px_rgba(0,210,255,0.25)] transition-transform hover:scale-105 duration-300">
          <img
            src={paths.assets.allayHeader}
            alt="Kvllay Allay Mascot"
            className="w-full h-auto object-contain"
          />
        </div>
      </Reveal>

      <Reveal direction="up" delay={160}>
        <h1 className="text-3xl sm:text-5xl lg:text-6xl font-extrabold text-[#F0F6FC] tracking-tight max-w-[840px] leading-[1.14] sm:leading-[1.12] mb-3 sm:mb-5">
          Fast. Lean.{" "}
          <span className="text-transparent bg-clip-text bg-gradient-to-r from-[#00D2FF] via-[#38BDF8] to-[#00E5A3]">
            Redis-Compatible.
          </span>
        </h1>
      </Reveal>

      <Reveal direction="up" delay={240}>
        <p className="text-sm sm:text-base lg:text-lg text-[#8B9BB4] max-w-[660px] leading-relaxed mb-7 sm:mb-9 px-1">
          A drop-in Redis RESP2 replacement written in modern C++17. Up to 5.67M req/s throughput, 87x smaller Docker image, 3.7x lighter RAM, and 100% native RESP2 compatibility.
        </p>
      </Reveal>

      <Reveal direction="up" delay={320} className="w-full flex justify-center">
        <div className="flex flex-wrap md:flex-nowrap items-center justify-center gap-2.5 sm:gap-3.5 mb-8 sm:mb-10 w-full max-w-[840px]">
          <a
            href={paths.quickstart}
            className="flex items-center justify-center gap-2 px-4.5 sm:px-6 py-2.5 sm:py-3.5 bg-[#00D2FF] hover:bg-[#00b8e6] text-[#08090E] rounded-xl text-[13px] sm:text-[15px] font-bold transition-all shadow-[0_0_25px_rgba(0,210,255,0.25)] hover:shadow-[0_0_30px_rgba(0,210,255,0.4)] shrink-0 whitespace-nowrap"
          >
            <span>Get Started</span>
            <IconArrowRight className="size-4 stroke-[2.5]" strokeWidth={2.5} />
          </a>

          <a
            href={links.github}
            target="_blank"
            rel="noreferrer"
            className="flex items-center justify-center gap-2 sm:gap-2.5 px-3.5 sm:px-5.5 py-2.5 sm:py-3.5 bg-[#0E131F] hover:bg-[#161D2E] border border-[#1E3B5C] rounded-xl text-[13px] sm:text-[15px] font-semibold text-[#F0F6FC] transition-colors shrink-0 whitespace-nowrap group"
          >
            <IconStar className="size-4 sm:size-4.5 text-[#EAB308] fill-[#EAB308] shrink-0" fill="currentColor" />
            <span>Star on GitHub</span>
            {isLoading ? (
              <span className="ml-1 inline-block w-10 h-5 rounded-full bg-[#1B2436] animate-pulse shrink-0" />
            ) : (
              <span className="ml-1 inline-flex items-center justify-center w-10 h-5 rounded-full bg-[#1B2436] text-xs font-mono text-[#00D2FF] group-hover:bg-[#223049] transition-colors shrink-0">
                {formatStars(stars)}
              </span>
            )}
          </a>

          <a
            href={paths.quickstart}
            className="flex items-center justify-center gap-2 px-3.5 sm:px-5 py-2.5 sm:py-3.5 bg-[#0E131F] hover:bg-[#161D2E] border border-[#1E3B5C] rounded-xl text-[13px] sm:text-[15px] font-medium text-[#8B9BB4] hover:text-[#F0F6FC] transition-colors shrink-0 whitespace-nowrap"
          >
            <IconDownload className="size-4 text-[#8B9BB4]" />
            <span>Standalone Binaries</span>
          </a>
        </div>
      </Reveal>

      <Reveal direction="up" delay={400} className="w-full flex justify-center">
        <div className="w-full max-w-[780px] flex items-center justify-between gap-3 px-3.5 sm:px-5 py-2.5 sm:py-3 bg-[#0A0E17] border border-[#1B2436] rounded-xl text-xs sm:text-sm font-mono shadow-inner group">
          <div className="flex items-center gap-2.5 sm:gap-3 overflow-x-auto scrollbar-none py-0.5">
            <span className="text-[#00D2FF] font-bold select-none">$</span>
            <span className="text-[#F0F6FC] whitespace-nowrap">{dockerCmd}</span>
          </div>
          <button
            type="button"
            onClick={handleCopy}
            className="shrink-0 p-1.5 text-[#546682] hover:text-[#00D2FF] rounded-md transition-colors flex items-center gap-1.5 focus:outline-none"
            title="Copy to clipboard"
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
      </Reveal>
    </section>
  )
}
