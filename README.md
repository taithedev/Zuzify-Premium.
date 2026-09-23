# Zuzify Premium

Zuzify Premium is a native Windows C++ / Dear ImGui application. It is separate from Zuzify Socials.

## Features

- Supabase email authentication
- Encrypted Windows "Keep me signed in" sessions
- Premium subscription state and expiry display
- Premium account controls
- Credit balance and lifetime credit tracking
- Server-authorized admin Premium and credit management
- Dark liquid-glass UI
- Purple, blue, pink and green theme presets
- Custom accent colors
- Adjustable glass intensity
- Live CPU and memory usage
- GPU name and VRAM information
- Application FPS and frame-time information
- GitHub Releases update checking
- One-click update download and executable replacement
- GitHub Actions Windows release builds
- Premium and admin status indicators

## Build

Open the repository folder in Visual Studio with CMake support, let CMake configure, then build ZuzifyPremium in Release or Debug.

The default local application version is 1.0.0. Release builds can override it with -DZUZIFY_VERSION=<tag>.

## Releases

Push a tag such as v1.0.1. GitHub Actions builds the Windows executable and publishes ZuzifyPremium.exe to the GitHub Release. The desktop updater checks the latest release and looks specifically for that asset.

## Supabase

The app uses only the Supabase publishable key. Service-role access stays inside the premium-admin Edge Function.

Database migrations live under supabase/migrations/, and the deployed Edge Function source is tracked under supabase/functions/premium-admin/.

## Security

Premium grants and credit changes are server-side operations. Row Level Security limits normal users to their own Premium settings, subscription state, and credit data.
