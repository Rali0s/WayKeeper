-- EPUB images sometimes advertise unusable physical DPI (for example, 1 dpi),
-- which makes TeX calculate dimensions larger than its numeric limit.  Apply a
-- deterministic page-width ceiling while preserving each image's aspect ratio.
function Image(image)
  image.attributes.width = "90%"
  image.attributes.height = nil
  return image
end
