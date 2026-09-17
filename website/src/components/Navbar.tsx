import { useState, useRef, useEffect } from "react"
import { IconBrandGithub, IconArrowRight, IconMenu2, IconX, IconStar } from "@tabler/icons-react"
import logoImg from "@root/logo.png"
import { useGithubStars, formatStars } from "@/lib/useGithubStars"
import { links } from "@/config/links"
import { paths } from "@/config/paths"

export function Navbar() {
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false)
  const { stars, isLoading } = useGithubStars()
  const headerRef = useRef<HTMLElement>(null)

  const navLinks = [
    { name: "Why Kvllay", href: paths.why },
    { name: "Benchmarks", href: paths.benchmarks },
    { name: "Features", href: paths.features },
    { name: "Quickstart", href: paths.quickstart },
    { name: "Docs", href: paths.docs },
  ]

  useEffect(() => {
    if (!mobileMenuOpen) return

    const handlePointerDown = (e: MouseEvent | TouchEvent) => {
      if (headerRef.current && !headerRef.current.contains(e.target as Node)) {
        setMobileMenuOpen(false)
      }
    }

    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        setMobileMenuOpen(false)
      }
    }

    document.addEventListener("mousedown", handlePointerDown)
    document.addEventListener("touchstart", handlePointerDown)
    window.addEventListener("keydown", handleKeyDown)

    return () => {
      document.removeEventListener("mousedown", handlePointerDown)
      document.removeEventListener("touchstart", handlePointerDown)
      window.removeEventListener("keydown", handleKeyDown)
    }
  }, [mobileMenuOpen])

  useEffect(() => {
    const handleResize = () => {
      if (window.innerWidth >= 768) {
        setMobileMenuOpen(false)
      }
    }
    window.addEventListener("resize", handleResize)
    return () => window.removeEventListener("resize", handleResize)
  }, [])

  return (
    <header
      ref={headerRef}
      className="sticky top-0 z-50 w-full bg-[#070A10]/90 backdrop-blur-md border-b border-[#1B2436]"
    >
      <div className="max-w-[1240px] mx-auto px-4 sm:px-6 lg:px-10 h-[64px] sm:h-[76px] flex items-center justify-between">
        <a href={paths.home} className="flex items-center gap-2.5 sm:gap-3 group focus:outline-none shrink-0">
          <div className="w-[32px] h-[32px] sm:w-[38px] sm:h-[38px] rounded-lg overflow-hidden flex items-center justify-center transition-transform group-hover:scale-105">
            <img src={logoImg} alt="kvllay logo" className="w-full h-full object-contain" />
          </div>
          <span className="text-[19px] sm:text-[22px] font-bold text-[#F0F6FC] tracking-tight">kvllay</span>
        </a>

        <nav className="hidden md:flex items-center gap-8">
          {navLinks.map((link) => (
            <a
              key={link.name}
              href={link.href}
              className="text-[14px] text-[#8B9BB4] hover:text-[#F0F6FC] transition-colors font-medium"
            >
              {link.name}
            </a>
          ))}
        </nav>

        <div className="hidden md:flex items-center gap-3.5 shrink-0">
          <a
            href={links.github}
            target="_blank"
            rel="noreferrer"
            className="flex items-center gap-2 px-3.5 py-2 bg-[#0E131F] hover:bg-[#161D2E] border border-[#1E3B5C] rounded-lg text-[13px] font-medium text-[#F0F6FC] transition-colors group shrink-0"
          >
            <IconBrandGithub className="size-4 text-[#F0F6FC] shrink-0" />
            <span>GitHub</span>
            {isLoading ? (
              <span className="w-[50px] h-5 rounded bg-[#1B2436] animate-pulse inline-block shrink-0" />
            ) : (
              <span className="inline-flex items-center justify-center gap-1 w-[50px] h-5 rounded bg-[#1B2436] text-[11px] font-mono text-[#00D2FF] group-hover:bg-[#223049] transition-colors shrink-0">
                <IconStar className="size-3 fill-[#00D2FF] text-[#00D2FF]" fill="currentColor" />
                <span>{formatStars(stars)}</span>
              </span>
            )}
          </a>
          <a
            href={paths.quickstart}
            className="flex items-center gap-1.5 px-4.5 py-2 bg-[#00D2FF] hover:bg-[#00b8e6] text-[#08090E] rounded-lg text-[13px] font-bold transition-all shadow-[0_0_20px_rgba(0,210,255,0.25)] hover:shadow-[0_0_25px_rgba(0,210,255,0.4)] shrink-0"
          >
            <span>Deploy Now</span>
            <IconArrowRight className="size-3.5 stroke-[2.5]" strokeWidth={2.5} />
          </a>
        </div>

        <button
          type="button"
          onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
          className="md:hidden p-1.5 sm:p-2 text-[#8B9BB4] hover:text-[#F0F6FC] focus:outline-none"
          aria-label="Toggle Navigation Menu"
        >
          {mobileMenuOpen ? <IconX className="size-5.5 sm:size-6" /> : <IconMenu2 className="size-5.5 sm:size-6" />}
        </button>
      </div>

      {mobileMenuOpen && (
        <div className="md:hidden absolute top-full left-0 right-0 w-full border-b border-[#1B2436] bg-[#0A0E17]/98 backdrop-blur-xl px-4 sm:px-6 py-4 sm:py-6 flex flex-col gap-4 shadow-2xl shadow-black/80 max-h-[calc(100dvh-64px)] sm:max-h-[calc(100dvh-76px)] overflow-y-auto animate-in slide-in-from-top-2 duration-200">
          <nav className="flex flex-col gap-2.5">
            {navLinks.map((link) => (
              <a
                key={link.name}
                href={link.href}
                onClick={() => setMobileMenuOpen(false)}
                className="text-[14px] sm:text-[15px] text-[#8B9BB4] hover:text-[#00D2FF] transition-colors py-1.5 font-medium"
              >
                {link.name}
              </a>
            ))}
          </nav>
          <div className="pt-3.5 border-t border-[#1B2436] flex flex-col sm:flex-row gap-2.5 sm:gap-3">
            <a
              href={links.github}
              target="_blank"
              rel="noreferrer"
              className="flex items-center justify-center gap-2 px-4 py-2 bg-[#0E131F] border border-[#1E3B5C] rounded-lg text-[13px] sm:text-[14px] font-medium text-[#F0F6FC]"
            >
              <IconBrandGithub className="size-4 text-[#F0F6FC]" />
              <span>Star on GitHub</span>
            </a>
            <a
              href={paths.quickstart}
              onClick={() => setMobileMenuOpen(false)}
              className="flex items-center justify-center gap-2 px-4 py-2 bg-[#00D2FF] text-[#08090E] rounded-lg text-[13px] sm:text-[14px] font-bold"
            >
              <span>Deploy Now</span>
              <IconArrowRight className="size-4 stroke-[2.5]" strokeWidth={2.5} />
            </a>
          </div>
        </div>
      )}
    </header>
  )
}
