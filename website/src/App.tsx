import { Navbar } from "@/components/Navbar"
import { Hero } from "@/components/Hero"
import { ShowcaseMetrics } from "@/components/ShowcaseMetrics"
import { Comparison } from "@/components/Comparison"
import { FeaturesBento } from "@/components/FeaturesBento"
import { CodeIntegration } from "@/components/CodeIntegration"
import { Quickstart } from "@/components/Quickstart"
import { Documentation } from "@/components/Documentation"
import { CtaBanner } from "@/components/CtaBanner"
import { Footer } from "@/components/Footer"

export default function App() {
  return (
    <div className="min-h-dvh bg-canvas text-text-primary selection:bg-accent-cyan/20 selection:text-accent-cyan flex flex-col items-center w-full">
      <Navbar />
      <main className="flex flex-col items-center w-full flex-1">
        <Hero />
        <ShowcaseMetrics />
        <Comparison />
        <FeaturesBento />
        <CodeIntegration />
        <Quickstart />
        <Documentation />
        <CtaBanner />
      </main>
      <Footer />
    </div>
  )
}