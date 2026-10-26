# AGENTS.md - Frontend Guide for kvllay Website

## Overview
The `website/` directory contains the modern landing page and documentation portal for `kvllay`. It is built with **React 19**, **TypeScript**, **Tailwind CSS v4**, and **Vite 8**, optimized for fast loading, dark-mode aesthetics, responsive design, and seamless presentation of benchmarks, code snippets, and quickstart instructions.

---

## Technology Stack
- **Framework**: React 19 (`react`, `react-dom`) with React Compiler (`babel-plugin-react-compiler`)
- **Build Tool**: Vite 8 with `@vitejs/plugin-react`
- **Styling**: Tailwind CSS v4 (`@tailwindcss/vite`, `tailwindcss`)
- **Icons**: Tabler Icons (`@tabler/icons-react`)
- **Fonts**: Inter (UI / sans-serif) & Geist Mono (code / monospace) loaded via Google Fonts in `index.html`
- **Language**: TypeScript with strict mode and ESLint React Hooks validation

---

## Directory Structure
```
website/
├── public/
│   ├── allay_docs.webp     # High-efficiency WebP animated mascot reading documentation
│   ├── allay_docs.gif      # Fallback GIF animation for docs card
│   └── allay_fly.webp      # Animated flying Allay mascot for CTA banner
├── src/
│   ├── components/
│   │   ├── Navbar.tsx          # Sticky header with navigation, version tag, and live GitHub stars
│   │   ├── Hero.tsx            # Hero section with interactive terminal console and quick CTA
│   │   ├── ShowcaseMetrics.tsx # 4-column KPI cards comparing RAM, binary size, and latency
│   │   ├── Comparison.tsx      # Benchmark slider with tabbed metrics (RAM, OPS, Latency)
│   │   ├── FeaturesBento.tsx   # Bento grid showcasing protocol, threading, and GC architecture
│   │   ├── CodeIntegration.tsx # Multi-language syntax-highlighted Redis client integration tabs
│   │   ├── Quickstart.tsx      # Equal-height installation cards (Docker, Binaries, Source)
│   │   ├── Documentation.tsx   # Guides overview, repo links, and animated mascot preview
│   │   ├── CtaBanner.tsx       # Bottom conversion block with animated mascot and star button
│   │   ├── Footer.tsx          # Navigation links, licenses, Docker Hub, and copyright
│   │   └── ui/
│   │       ├── button.tsx      # Base UI button with variants
│   │       └── Reveal.tsx      # Scroll reveal wrapper with directions, delays, and GPU transitions
│   ├── config/
│   │   ├── links.ts            # Centralized external links and repository URLs
│   │   ├── images.ts           # Centralized source asset URLs and public image URLs
│   │   └── paths.ts            # Centralized internal paths, section IDs, and public asset URLs
│   ├── lib/
│   │   ├── useGithubStars.ts   # Hook for GitHub API star fetching, caching, and skeleton state
│   │   └── useScrollReveal.ts  # IntersectionObserver hook with reduced-motion support for scroll animations
│   ├── App.tsx                 # Root application component orchestrating all page sections
│   ├── index.css               # Global styles, Tailwind v4 import, scrollbars, and font declarations
│   └── main.tsx                # Client entrypoint mounting React tree
├── index.html              # HTML shell with meta tags, SEO title, fonts, and favicon links
├── package.json            # Scripts and project dependencies
├── tsconfig.json           # Root TypeScript project references
├── tsconfig.app.json       # Frontend TypeScript compiler configuration with path aliases
├── vite.config.ts          # Vite build config with path aliases and filesystem permissions
└── AGENTS.md               # This technical guide for AI agents and developers
```

---

## Component Architecture & Responsibilities

All visual sections are divided into isolated, reusable components inside `src/components/` and assembled sequentially in `src/App.tsx`:

### 1. `Navbar.tsx`
- **Purpose**: Sticky navigation header with blurred translucent backdrop.
- **Key Features**:
  - Logo and brand title linking to top.
  - Version badge (`v1.2.0`) pointing to project releases.
  - Smooth anchor links (`#benchmarks`, `#features`, `#code`, `#install`, `#docs`).
  - GitHub star button consuming `useGithubStars()` with an animated pulse skeleton loader.
  - Mobile responsive drawer menu with hamburger toggle.

### 2. `Hero.tsx`
- **Purpose**: First-fold visual introduction to `kvllay`.
- **Key Features**:
  - Pill badge with glow highlight (`90x lighter than Redis • 2.1 MB RAM`).
  - Main headline and descriptive subtitle.
  - Quick action CTA buttons (Install Guide, Star on GitHub with skeleton loader).
  - Interactive terminal replica matching the original design:
    - Traffic light dots, copy-to-clipboard button, and simulated live latency badge.
    - Tabbed interactive queries (`SET user:1`, `GET user:1`, `TTL user:1`).

### 3. `ShowcaseMetrics.tsx`
- **Purpose**: High-contrast grid of primary performance indicators.
- **Key Metrics**:
  - Baseline RAM: **2.1 MB** (Kvllay) vs **180 MB** (Redis) — 98.8% reduction.
  - Binary Size: **1.6 MB** (Kvllay) vs **142 MB** (Redis) — Scratch Docker image.
  - Client Compatibility: **100%** Redis RESP2 standard protocol.
  - Concurrency: Multi-threaded (`std::shared_mutex`) vs Single-threaded event loop.

### 4. `Comparison.tsx`
- **Purpose**: Interactive benchmark data explorer.
- **Key Features**:
  - Tabbed metrics switcher: *Memory Footprint*, *Read OPS (GET)*, *Write OPS (SET)*, *Latency p99*.
  - Visual comparison bars with exact measurements and percentage deltas.
  - Test environment specification callout (`redis-benchmark -c 50 -n 100000`).

### 5. `FeaturesBento.tsx`
- **Purpose**: Modern Bento-grid displaying core technical differentiators.
- **Key Features**:
  - Architecture breakdown: RESP2 parser, zero-alloc token streaming.
  - Memory model: `std::shared_mutex` read/write locking.
  - Eviction: Passive lookup TTL checks and active sweep garbage collector.
  - Cross-platform networking: POSIX BSD sockets and Windows Winsock.

### 6. `CodeIntegration.tsx`
- **Purpose**: Demonstrates drop-in replacement across 5 major programming languages.
- **Key Features**:
  - Language tabs: Python (`redis-py`), Node.js (`ioredis`), Go (`go-redis`), Rust (`redis-rs`), Bash (`redis-cli`).
  - Precise syntax highlighting implemented with semantic tokens (`keyword`, `string`, `function`, `comment`, `number`).
  - Copy snippet button with checkmark feedback.

### 7. `Quickstart.tsx`
- **Purpose**: Installation instructions with uniform card layout.
- **Key Features**:
  - Three equalized cards (`h-full flex flex-col justify-between`):
    1. **Docker Container**: Instant `docker run -d -p 6379:6379 kenyka/kvllay` and `compose.yml`.
    2. **Standalone Binary**: Precompiled Linux/Windows executables with direct download button.
    3. **Compile from Source**: Single-line C++17 build with `make compile` or manual `g++`.

### 8. `Documentation.tsx`
- **Purpose**: Direct access to repository documentation and architecture specifications.
- **Key Features**:
  - Feature highlights for English (`docs/en.md`) and Russian (`docs/ru.md`) guides.
  - Action buttons linking directly to GitHub repository markdown documents.
  - Visual mascot card featuring animated Allay reading docs (`allay_docs.webp` with fallback `allay_docs.gif`).
  - Custom caption and terminal header.

### 9. `CtaBanner.tsx`
- **Purpose**: High-converting footer banner before closing navigation.
- **Key Features**:
  - Floating animated Allay mascot (`allay_fly.webp`).
  - Primary CTA button with live GitHub star count and skeleton loader.
  - Secondary links to GitHub Releases and documentation.

### 10. `Footer.tsx`
- **Purpose**: Comprehensive site footer.
- **Key Features**:
  - Brand identity, mission statement, and MIT license disclaimer.
  - Categorized links: Product, Documentation, Community & Ecosystem.
  - Direct Docker Hub and Redis RESP2 specification links.

---

## Custom Hooks & Utilities

### `useGithubStars` (`src/lib/useGithubStars.ts`)
- **Hook**: `useGithubStars(owner = "thekeny", repo = "kvllay")`
- **Returns**: `{ stars: number, isLoading: boolean }`
- **Mechanism**:
  - Checks `sessionStorage` for cached star count and timestamp.
  - Cache TTL is 1 hour (`3600000` ms) to prevent hitting GitHub API rate limits.
  - Initializes state using synchronous initializer function (`useState(getCachedStars)`) to satisfy React Compiler invariants.
  - Fetches fresh data from `https://api.github.com/repos/{owner}/{repo}` asynchronously.
  - If fetch fails or rate limit is reached, gracefully falls back to cached value or sensible default (`42`).
  - Helper `formatStars(count)`: Formats numbers (e.g. `1250` -> `"1.3k"`).

### `useScrollReveal` & `Reveal` (`src/lib/useScrollReveal.ts`, `src/components/ui/Reveal.tsx`)
- **Hook**: `useScrollReveal<T>({ threshold, rootMargin, once })`
  - High-performance `IntersectionObserver` observing DOM visibility.
  - Respects OS `prefers-reduced-motion` settings.
  - Unobserves elements once revealed (`once = true`) to minimize CPU/runtime overhead.
- **Component**: `<Reveal direction="..." delay={ms} duration={ms} distance={px} once={true} as="...">`
  - Directions: `up`, `down`, `left`, `right`, `scale`, `fade`.
  - Staggered cascading appearance via `delay={idx * ms}` on grids and lists.
  - Pure GPU-accelerated CSS transitions (`opacity`, `transform`) using spring-style `cubic-bezier(0.16, 1, 0.3, 1)`.

---

## Asset Resolution & Static Files

Vite is configured with path aliases in `vite.config.ts` and `tsconfig.app.json`:
- `@/*` -> `website/src/*` (application source code)
- `@root/*` -> `../` (repository root, e.g. for `logo.png`, `logo-16x16.png`, `logo.ico`)
- `@docs/*` -> `../docs/*` (repository documentation and benchmark assets)

Static files placed in `website/public/` are served at the root URL:
- `/allay_docs.webp` & `/allay_docs.gif`: Documentation card animation.
- `/allay_fly.webp`: Flying Allay mascot for the bottom CTA banner.

Image sources used by the website are collected in `src/config/images.ts`; import them from `@/config/images` instead of declaring image paths in components. Images in the repository root and `docs/images/` are resolved directly from their source files with Vite URL imports. Keep website-only assets in `website/public/`.

Favicons in `index.html` resolve directly from the repository root:
- `../logo.ico`
- `../logo.png`
- `../logo-16x16.png`

---

## Styling & Design Tokens

- **Surface separation**: Use background tone and spacing. Do not add borders, divider lines, decorative rings, gradients, or resting shadows, including on hover and selected states. The hero mascot's subtle cyan glow and the `Redis-Compatible` text gradient are intentional user-requested exceptions.
- **Interaction states**: Selected tabs use a cyan fill with dark text. Keyboard focus uses a contrasting light fill with dark text through the shared `:focus-visible` styles, without an outline or ring. Keep button targets at least 44×44 px.
- **Responsive navigation**: The compact menu is used below 1024 px; desktop navigation and the resize handler must use the same breakpoint.
- **Type scale**: Body descriptions are 16 px; secondary labels are at least 13 px. Reserve monospace for code and commands; use tabular numerals for changing metrics.
- **Tailwind Version**: Tailwind CSS v4 loaded via `@import "tailwindcss";` in `src/index.css`.
- **Theme Palette**:
  - Background primary: `#070A10`
  - Card background: `#0E131F` / `#0A0E17`
  - Border dark: `#1B2436`
  - Border accent: `#1E3B5C`
  - Cyan brand primary: `#00D2FF`
  - Mint / green accent: `#00E5A3`
  - Text primary: `#F0F6FC`
  - Text secondary: `#8B9BB4`
  - Text muted: `#546682`
- **Typography**:
  - UI Font: `Inter`, system-ui, sans-serif
  - Monospace Font: `Geist Mono`, monospace

---

## Development & Build Workflow

Run commands from the `website/` directory:

```bash
# Start development server (default http://localhost:5173)
npm run dev

# Run TypeScript type check and production bundle build
npm run build

# Run ESLint validation
npm run lint

# Preview production build locally
npm run preview
```

---

## Rules and Invariants for Agents

1. **No Stray Comments in Source Files**:
   - Do not leave developer comments (`// ...`, `/* ... */`, `{/* ... */}`) in `src/` files or configuration files. Keep code clean and self-documenting.
2. **Preserve User Captions & Customizations**:
   - In `Documentation.tsx`, do not overwrite user custom captions (such as `"You sure you don't want to try kvllay? >:3"` and `mind_allay.gif`).
3. **Skeleton Loading for Async States**:
   - Any UI displaying GitHub star counts must render a pulse skeleton loader while `isLoading` is true.
4. **React Compiler Compatibility**:
   - Do not trigger synchronous `setState` directly inside `useEffect`. Use state initializer functions for cached state retrieval.
5. **Component Modularity**:
   - Every standalone page section must reside in its own component file under `src/components/`. `App.tsx` should only import and lay them out.
6. **Responsive Layouts**:
   - Ensure all grids, flex wraps, and cards gracefully adapt down to mobile screens (`360px`+). Quickstart cards must maintain equal height across columns using flexbox.
