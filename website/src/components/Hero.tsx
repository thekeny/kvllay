import { useState } from "react"
import { IconArrowRight, IconStar, IconDownload, IconCopy, IconCheck } from "@tabler/icons-react"
import { useGithubStars, formatStars } from "@/lib/useGithubStars"
import { links } from "@/config/links"
import { paths } from "@/config/paths"
import { images } from "@/config/images"
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
      <Reveal direction="scale" delay={80}>
        <div className="relative w-28 sm:w-44 lg:w-48 h-auto mb-4 sm:mb-6 drop-shadow-[0_10px_35px_rgba(0,210,255,0.25)] transition-transform hover:scale-105 duration-300">
          <img
            src={images.allayHeader}
            alt="Kvllay Allay Mascot"
            className="w-full h-auto object-contain"
          />
        </div>
      </Reveal>

      <Reveal direction="up" delay={160}>
        <h1 className="text-3xl sm:text-5xl lg:text-6xl font-extrabold text-text-primary tracking-tight max-w-[920px] text-balance leading-[1.14] sm:leading-[1.12] mb-3 sm:mb-5">
          Fast. Lean.{" "}
          <span className="text-transparent bg-clip-text bg-linear-to-r from-[#00D2FF] via-[#38BDF8] to-[#00E5A3]">
            Redis-Compatible.
          </span>
        </h1>
      </Reveal>

      <Reveal direction="up" delay={240}>
        <p className="text-base lg:text-lg text-text-secondary max-w-[660px] leading-relaxed mb-7 sm:mb-9 px-1">
          A drop-in Redis RESP2 replacement written in modern C++17. Up to 5.67M req/s throughput, 87x smaller Docker image, 3.7x lighter RAM, and 100% native RESP2 compatibility.
        </p>
      </Reveal>

      <Reveal direction="up" delay={320} className="w-full flex justify-center">
        <div className="flex flex-wrap md:flex-nowrap items-center justify-center gap-2.5 sm:gap-3.5 mb-8 sm:mb-10 w-full max-w-[840px]">
          <a
            href={paths.quickstart}
            className="flex items-center justify-center gap-2 px-4.5 sm:px-6 py-2.5 sm:py-3.5 bg-accent-cyan hover:bg-[#00b8e6] text-primary-foreground rounded-xl text-[13px] sm:text-[15px] font-bold transition-colors shrink-0 whitespace-nowrap"
          >
            <span>Install kvllay</span>
            <IconArrowRight className="size-4 stroke-[2.5]" strokeWidth={2.5} />
          </a>

          <a
            href={links.github}
            target="_blank"
            rel="noreferrer"
            className="flex items-center justify-center gap-2 sm:gap-2.5 px-3.5 sm:px-5.5 py-2.5 sm:py-3.5 bg-surface-card hover:bg-[#161D2E] rounded-xl text-[13px] sm:text-[15px] font-semibold text-text-primary transition-colors shrink-0 whitespace-nowrap group"
          >
            <IconStar className="size-4 sm:size-4.5 text-[#EAB308] fill-[#EAB308] shrink-0" fill="currentColor" />
            <span>Star on GitHub</span>
            {isLoading ? (
              <span className="ml-1 inline-block w-10 h-5 rounded-full bg-[#1B2436] animate-pulse shrink-0" />
            ) : (
              <span className="ml-1 inline-flex items-center justify-center w-10 h-5 rounded-full bg-[#1B2436] text-[13px] tabular-nums text-accent-cyan group-hover:bg-[#223049] transition-colors shrink-0">
                {formatStars(stars)}
              </span>
            )}
          </a>

          <a
            href={paths.quickstart}
            className="flex items-center justify-center gap-2 px-3.5 sm:px-5 py-2.5 sm:py-3.5 bg-surface-card hover:bg-[#161D2E] rounded-xl text-[13px] sm:text-[15px] font-medium text-text-secondary hover:text-text-primary transition-colors shrink-0 whitespace-nowrap"
          >
            <IconDownload className="size-4 text-text-secondary" />
            <span>Standalone Binaries</span>
          </a>
        </div>
      </Reveal>

      <Reveal direction="up" delay={400} className="w-full flex justify-center">
        <div className="w-full max-w-[780px] flex items-center justify-between gap-3 px-3.5 sm:px-5 py-2.5 sm:py-3 bg-surface-subtle rounded-xl text-[13px] sm:text-sm font-mono group">
          <div className="flex items-center gap-2.5 sm:gap-3 overflow-x-auto scrollbar-none py-0.5">
            <code className="text-accent-cyan font-bold select-none">$</code>
            <code className="text-text-primary whitespace-nowrap">{dockerCmd}</code>
          </div>
          <button
            type="button"
            onClick={handleCopy}
            className="shrink-0 p-1.5 text-text-secondary hover:text-accent-cyan rounded-md transition-colors flex items-center gap-1.5 focus:outline-none"
            title="Copy to clipboard"
          >
            {copied ? (
              <>
                <IconCheck className="size-4 text-accent-cyan" />
                <span className="text-[13px] text-accent-cyan font-sans font-medium hidden sm:inline">Copied!</span>
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
