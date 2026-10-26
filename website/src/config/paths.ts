const base = (import.meta.env.BASE_URL || "/").endsWith("/")
  ? (import.meta.env.BASE_URL || "/")
  : `${import.meta.env.BASE_URL}/`

export const paths = {
  root: base,
  home: "#",
  why: "#why",
  benchmarks: "#benchmarks",
  features: "#features",
  integrations: "#integrations",
  quickstart: "#quickstart",
  docs: "#docs",
  sections: {
    why: "why",
    benchmarks: "benchmarks",
    features: "features",
    integrations: "integrations",
    quickstart: "quickstart",
    docs: "docs",
  },
} as const

export default paths
