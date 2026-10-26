import { IconStar, IconDownload, IconBook2 } from "@tabler/icons-react"
import { useGithubStars, formatStars } from "@/lib/useGithubStars"
import { links } from "@/config/links"
import { paths } from "@/config/paths"
import { images } from "@/config/images"
import { Reveal } from "@/components/ui/Reveal"

export function CtaBanner() {
  const { stars, isLoading } = useGithubStars()

  return (
    <section className="w-full py-8 sm:py-12 px-4 sm:px-6 lg:px-10 max-w-[1240px] mx-auto">
      <Reveal direction="scale" delay={50} className="w-full">
        <div className="w-full bg-surface-card rounded-xl sm:rounded-2xl p-6 sm:p-12 lg:p-14 flex flex-col items-center text-center gap-4 sm:gap-6 relative overflow-hidden">

        <div className="relative w-20 sm:w-28 h-auto object-contain">
          <img
            src={images.allayFly}
            alt="Kvllay Flying Allay Animated Mascot"
            className="w-full h-auto object-contain"
          />
        </div>


        <div className="flex flex-col items-center gap-2.5 sm:gap-3 max-w-[760px]">
          <h2 className="text-xl sm:text-3xl lg:text-4xl font-bold text-text-primary tracking-tight leading-tight">
            Ready for a 90x Lighter, Blazing-Fast Cache?
          </h2>
          <p className="text-base text-text-secondary leading-[1.6]">
            Join developers swapping out heavy Redis containers for Kvllay in local dev environments, microservices, and edge computing. Zero migration code required. Open source under the MIT license.
          </p>
        </div>

        <div className="flex flex-wrap items-center justify-center gap-2.5 sm:gap-3.5 pt-1 sm:pt-2 w-full">
          <a
            href={links.github}
            target="_blank"
            rel="noreferrer"
            className="flex items-center justify-center gap-2.5 px-4.5 sm:px-6 py-2.5 sm:py-3.5 bg-accent-cyan hover:bg-[#00b8e6] text-primary-foreground rounded-xl text-[13.5px] sm:text-[15px] font-bold transition-colors w-full sm:w-auto"
          >
            <IconStar className="size-4.5 fill-[#08090E] shrink-0" fill="currentColor" />
            <span>Star on GitHub</span>
            {isLoading ? (
              <span className="ml-1 inline-block w-10 h-5 rounded-full bg-primary-foreground/20 animate-pulse shrink-0" />
            ) : (
              <span className="ml-1 inline-flex items-center justify-center w-10 h-5 rounded-full bg-primary-foreground/20 text-[13px] tabular-nums text-primary-foreground font-bold shrink-0">
                {formatStars(stars)}
              </span>
            )}
          </a>

          <a
            href={links.releases}
            target="_blank"
            rel="noreferrer"
            className="flex items-center justify-center gap-2 px-4.5 sm:px-5 py-2.5 sm:py-3.5 bg-surface-subtle hover:bg-[#161D2E] rounded-xl text-[13.5px] sm:text-[14px] font-medium text-text-primary transition-colors w-full sm:w-auto"
          >
            <IconDownload className="size-4 text-text-primary" />
            <span>GitHub Releases</span>
          </a>

          <a
            href={links.readme}
            target="_blank"
            rel="noreferrer"
            className="flex items-center justify-center gap-2 px-4.5 sm:px-5 py-2.5 sm:py-3.5 bg-surface-subtle hover:bg-[#161D2E] rounded-xl text-[13.5px] sm:text-[14px] font-medium text-text-secondary hover:text-text-primary transition-colors w-full sm:w-auto"
          >
            <IconBook2 className="size-4 text-text-secondary" />
            <span>Documentation (RU / EN)</span>
          </a>
        </div>
      </div>
      </Reveal>
    </section>
  )
}
