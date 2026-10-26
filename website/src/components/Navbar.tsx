import { useState, useRef, useEffect } from "react"
import { IconBrandGithub, IconMenu2, IconX, IconStar } from "@tabler/icons-react"
import { images } from "@/config/images"
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
      if (window.innerWidth >= 1024) {
        setMobileMenuOpen(false)
      }
    }
    window.addEventListener("resize", handleResize)
    return () => window.removeEventListener("resize", handleResize)
  }, [])

  return (
    <header
      ref={headerRef}
      className="sticky top-0 z-50 w-full bg-canvas"
    >
      <div className="max-w-[1240px] mx-auto px-4 sm:px-6 lg:px-10 h-[64px] sm:h-[76px] flex items-center justify-between lg:grid lg:grid-cols-[1fr_auto_1fr] lg:gap-4">
        <a href={paths.home} className="flex items-center gap-2.5 sm:gap-3 group focus:outline-none shrink-0 justify-self-start">
          <div className="w-[32px] h-[32px] sm:w-[38px] sm:h-[38px] rounded-lg overflow-hidden flex items-center justify-center transition-transform group-hover:scale-105">
            <img src={images.logo} alt="kvllay logo" className="w-full h-full object-contain" />
          </div>
          <span className="text-[19px] sm:text-[22px] font-bold text-text-primary tracking-tight">kvllay</span>
        </a>

        <nav className="hidden lg:flex items-center justify-center gap-3 xl:gap-6">
          {navLinks.map((link) => (
            <a
              key={link.name}
              href={link.href}
              className="inline-flex items-center justify-center text-[14px] text-text-secondary hover:text-text-primary transition-colors font-medium whitespace-nowrap"
            >
              {link.name}
            </a>
          ))}
        </nav>

        <div className="hidden lg:flex items-center justify-self-end gap-2 xl:gap-3.5 shrink-0">
          <a
            href={links.github}
            target="_blank"
            rel="noreferrer"
            className="flex items-center gap-2 px-3.5 py-2 bg-surface-card hover:bg-[#161D2E] rounded-lg text-[13px] font-medium text-text-primary transition-colors group shrink-0"
          >
            <IconBrandGithub className="size-4 text-text-primary shrink-0" />
            <span>GitHub</span>
            {isLoading ? (
              <span className="w-[50px] h-5 rounded bg-[#1B2436] animate-pulse inline-block shrink-0" />
            ) : (
              <span className="inline-flex items-center justify-center gap-1 w-[50px] h-5 rounded bg-[#1B2436] text-[13px] tabular-nums text-accent-cyan group-hover:bg-[#223049] transition-colors shrink-0">
                <IconStar className="size-3 fill-[#00D2FF] text-accent-cyan" fill="currentColor" />
                <span>{formatStars(stars)}</span>
              </span>
            )}
          </a>
          <a
            href={links.dockerHub}
            target="_blank"
            rel="noreferrer"
            className="flex items-center gap-1.5 px-4.5 py-2 bg-accent-cyan hover:bg-[#00b8e6] text-primary-foreground rounded-lg text-[13px] font-bold transition-colors shrink-0"
          >
            <img src={images.docker} alt="" className="size-4 object-contain" />
            <span>Docker</span>
          </a>
        </div>

        <button
          type="button"
          onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
          className="lg:hidden p-1.5 sm:p-2 text-text-secondary hover:text-text-primary focus:outline-none"
          aria-label="Toggle Navigation Menu"
          aria-expanded={mobileMenuOpen}
          aria-controls="mobile-navigation"
        >
          {mobileMenuOpen ? <IconX className="size-5.5 sm:size-6" /> : <IconMenu2 className="size-5.5 sm:size-6" />}
        </button>
      </div>

      {mobileMenuOpen && (
        <div id="mobile-navigation" className="lg:hidden absolute top-full left-0 right-0 w-full bg-surface-subtle px-4 sm:px-6 py-4 sm:py-6 flex flex-col gap-4 max-h-[calc(100dvh-64px)] sm:max-h-[calc(100dvh-76px)] overflow-y-auto animate-in slide-in-from-top-2 duration-200">
          <nav className="flex flex-col gap-2.5">
            {navLinks.map((link) => (
              <a
                key={link.name}
                href={link.href}
                onClick={() => setMobileMenuOpen(false)}
                className="flex items-center text-[14px] sm:text-[15px] text-text-secondary hover:text-accent-cyan transition-colors py-1.5 font-medium"
              >
                {link.name}
              </a>
            ))}
          </nav>
          <div className="pt-3.5 flex flex-col sm:flex-row gap-2.5 sm:gap-3">
            <a
              href={links.github}
              target="_blank"
              rel="noreferrer"
              className="flex items-center justify-center gap-2 px-4 py-2 bg-surface-card rounded-lg text-[13px] sm:text-[14px] font-medium text-text-primary"
            >
              <IconBrandGithub className="size-4 text-text-primary" />
              <span>Star on GitHub</span>
            </a>
            <a
              href={links.dockerHub}
              target="_blank"
              rel="noreferrer"
              onClick={() => setMobileMenuOpen(false)}
              className="flex items-center justify-center gap-2 px-4 py-2 bg-accent-cyan text-primary-foreground rounded-lg text-[13px] sm:text-[14px] font-bold"
            >
              <img src={images.docker} alt="" className="size-4" />
              <span>Docker</span>
            </a>
          </div>
        </div>
      )}
    </header>
  )
}
