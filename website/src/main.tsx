import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import './index.css'
import App from './App.tsx'
import { images } from './config/images'

document.querySelector<HTMLLinkElement>('link[rel="icon"][type="image/x-icon"]')!.href = images.favicon
document.querySelector<HTMLLinkElement>('link[rel="icon"][sizes="32x32"]')!.href = images.logo
document.querySelector<HTMLLinkElement>('link[rel="icon"][sizes="16x16"]')!.href = images.favicon16

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
)
