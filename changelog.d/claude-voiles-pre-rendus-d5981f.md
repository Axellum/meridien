### Changed

- **Faster home-screen repaint (curve veils baked)**: the translucent veils under the hourly temperature curve and between the
  five-day highs and lows are now drawn once into an image (PSRAM) when the weather, layout or theme changes, then laid down in
  one draw per screen band instead of one gradient column per 8 px, filled line by line. The sky gradient hidden under the sky
  image is no longer computed. Same pixels: the PC render bench gives identical images on the home, themes, time-zone and
  alert scenes, at 1280×720, 1024×600 and 720×1280. Not tested on board (repaint time and extra PSRAM not measured there).
