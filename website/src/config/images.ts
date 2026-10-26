export const images = {
  logo: new URL("../../../logo.png", import.meta.url).href,
  favicon: new URL("../../../logo.ico", import.meta.url).href,
  favicon16: new URL("../../../logo-16x16.png", import.meta.url).href,
  docker: "./docker.png",
  allayHeader: "./allay_header.webp",
  allayFly: "./allay_fly.webp",
  allayDocsWebp: "./allay_docs.webp",
  allayDocsGif: "./allay_docs.gif",
  benchmarkSingleClient: new URL("../../../docs/images/benchmark_single_client.png", import.meta.url).href,
  benchmarkMultithreaded: new URL("../../../docs/images/benchmark_multithreaded.png", import.meta.url).href,
  benchmarkRam: new URL("../../../docs/images/benchmark_ram.png", import.meta.url).href,
  benchmarkDocker: new URL("../../../docs/images/benchmark_docker.png", import.meta.url).href,
  benchmarkLatency: new URL("../../../docs/images/benchmark_latency.png", import.meta.url).href,
} as const

export default images
