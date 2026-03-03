/*-----------------------------------------------------------------------------
 * DVB Subtitle Player - Subtitle Display Widget Implementation
 *---------------------------------------------------------------------------*/
/**
 * @file SubtitleWidget.cpp
 * @brief Implementation of SubtitleWidget for DVB Subtitle Player
 *
 * @section pixel_format_conversion Pixel Format Conversion
 *
 * The decoder provides pixmap data in different formats. This widget handles:
 *
 * @subsection argb32 ARGB32 Format
 *
 * 32-bit ARGB format with byte order: A-R-G-B (big-endian notation)
 * Qt's Format_ARGB32 stores: B-G-R-A in memory (little-endian)
 *
 * @subsection palette8bit PALETTE8BIT Format
 *
 * 8-bit palette indices that must be looked up in CLUT (Color Look-Up Table).
 * Each CLUT entry contains 4 bytes: Y, Cb, Cr, T (transparency)
 * These must be converted to RGB using YCbCr→RGB conversion.
 *
 * @code
 * // YCbCr to RGB conversion (ITU-R BT.601)
 * R = Y + 1.402 * (Cr - 128)
 * G = Y - 0.344 * (Cb - 128) - 0.714 * (Cr - 128)
 * B = Y + 1.772 * (Cb - 128)
 * @endcode
 *
 * @section color_lookup Color Lookup from CLUT
 *
 * When processing PALETTE8BIT pixmaps, the palette data contains:
 *
 * @code
 * struct CLUTEntry {
 *     uint8_t Y;      // Luminance (0-255)
 *     uint8_t Cb;     // Chrominance blue (0-255)
 *     uint8_t Cr;     // Chrominance red (0-255)
 *     uint8_t T;      // Transparency (0-255, 0=fully transparent)
 * };
 * @endcode
 *
 * The YCbCr values are converted to RGB, then combined with transparency
 * to produce ARGB32 pixels for Qt rendering.
 *
 * @section region_intersection Region Intersection for Clearing
 *
 * When clearRegion() is called, it must remove all regions that intersect
 * with the clear rectangle. Qt's QRect::intersects() method handles this:
 *
 * @code
 * void clearRegion(int left, int top, int right, int bottom) {
 *     QRect clearRect(left, top, right - left, bottom - top);
 *     QMutexLocker locker(&m_mutex);
 *     for (int i = m_regions.size() - 1; i >= 0; i--) {
 *         if (clearRect.intersects(m_regions[i].originalRect)) {
 *             m_regions.removeAt(i);
 *         }
 *     }
 *     update();
 * }
 * @endcode
 *
 * @section aspect_ratio_aspect_ratio Aspect Ratio and Letterbox/Pillarbox
 *
 * The calculateDisplayRect() method implements "contain" scaling:
 *
 * - If window is wider than source: pillarbox (black bars on sides)
 * - If window is taller than source: letterbox (black bars on top/bottom)
 *
 * This ensures subtitles maintain their original aspect ratio and position.
 */

#include "SubtitleWidget.h"
#include <QDebug>
#include <QPainter>
#include <QMutexLocker>
#include <QCoreApplication>

SubtitleWidget::SubtitleWidget(QWidget* parent)
  : QWidget(parent)
  , m_displayWidth(DEFAULT_DVB_WIDTH)
  , m_displayHeight(DEFAULT_DVB_HEIGHT)
  , m_scaleX(1.0)
  , m_scaleY(1.0)
{
  setAutoFillBackground(true);
  clear();
}


SubtitleWidget::~SubtitleWidget()
{
}


void
SubtitleWidget::setDisplaySize(int width, int height)
{
  m_displayWidth = width;
  m_displayHeight = height;
  update();
}


QRect
SubtitleWidget::calculateDisplayRect(const QRect& originalRect) const
{
  // Calculate scale factors based on current window size vs source display resolution
  // Use "letterbox" scaling: maintain aspect ratio, fit within window
  double windowAspect = (double)width() / height();
  double sourceAspect = (double)m_displayWidth / m_displayHeight;
  double scale;
  int offsetX = 0;
  int offsetY = 0;

  if (windowAspect > sourceAspect)
  {
    // Window is wider than source aspect ratio - pillarbox
    scale = (double)height() / m_displayHeight;
    offsetX = (width() - (int)(m_displayWidth * scale)) / 2;
    offsetY = 0;
  }
  else
  {
    // Window is taller than source aspect ratio - letterbox
    scale = (double)width() / m_displayWidth;
    offsetX = 0;
    offsetY = (height() - (int)(m_displayHeight * scale)) / 2;
  }

  // Scale and center the subtitle region
  int scaledX = (int)(originalRect.x() * scale) + offsetX;
  int scaledY = (int)(originalRect.y() * scale) + offsetY;
  int scaledW = (int)(originalRect.width() * scale);
  int scaledH = (int)(originalRect.height() * scale);

  return QRect(scaledX, scaledY, scaledW, scaledH);
}


void
SubtitleWidget::clear()
{
  QMutexLocker locker(&m_mutex);

  m_regions.clear();
  m_paletteData.clear();
  update();
}


void
SubtitleWidget::clearRegion(int left, int top, int right, int bottom)
{
  QRect clearRect(left, top, right - left, bottom - top);
  QMutexLocker locker(&m_mutex);

  // Remove all regions that intersect with the specified rectangle
  // Match against ORIGINAL coordinates (from decoder), not display coordinates
  for (int i = m_regions.size() - 1; i >= 0; i--)
  {
    if (clearRect.intersects(m_regions[i].originalRect))
    {
      m_regions.removeAt(i);
    }
  }

  // Update the display to show cleared regions
  // Mutex is released before update() returns, so this is safe
  if (!m_regions.isEmpty())
  {
    locker.unlock();
    update();
  }
  else
  {
    // No regions left, clear everything
    locker.unlock();
    clear();
  }
}


void
SubtitleWidget::startNewSubtitle(int subtitleCount)
{
  // Clear previous regions for new subtitle
  m_regions.clear();
  // Note: This is no longer used since cleanRegionCallback handles clearing
  // Keeping method for API compatibility
  Q_UNUSED(subtitleCount);
}


void
SubtitleWidget::finishSubtitle()
{
  // All regions added, trigger update to display them
  // Note: This is no longer used, keeping for API compatibility
}


void
SubtitleWidget::renderPixmap(const LS_Pixmap_t* pixmap, const uint8_t* palette)
{
  if (!pixmap ||
      !pixmap->data)
  {
    return;
  }

  // Store palette if provided
  if (palette)
  {
    // Assume 256-entry CLUT (256 * 4 bytes per LS_ColorRGB_t entry)
    m_paletteData = QByteArray(reinterpret_cast<const char*>(palette), 256 * 4);
  }
  else
  {
    m_paletteData.clear();
  }

  // Convert the pixmap to a QImage
  QImage displayImage = convertPixmapToImage(pixmap);
  // Store ORIGINAL coordinates (from decoder) for clearRegion matching and scaling
  QRect originalRect(pixmap->leftPos, pixmap->topPos, pixmap->width, pixmap->height);
  // Add this region to the list (don't overwrite previous regions!)
  SubtitleRegion region;

  region.image = displayImage;
  region.originalRect = originalRect;

  QMutexLocker locker(&m_mutex);

  m_regions.append(region);
  // Don't call update() here - wait until finishSubtitle() is called
}


void
SubtitleWidget::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);

  QPainter painter(this);

  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  // Fill background with black
  painter.fillRect(rect(), QColor(0, 0, 0));

  // Copy regions while holding mutex (thread-safe read)
  QVector<SubtitleRegion> regionsCopy;
  {
    QMutexLocker locker(&m_mutex);

    regionsCopy = m_regions;
  }

  // Draw all regions for current subtitle (mutex released during painting)
  for (const SubtitleRegion& region : regionsCopy)
  {
    if (!region.image.isNull())
    {
      // Calculate scaled display position based on current window size
      QRect displayRect = calculateDisplayRect(region.originalRect);
      // Scale the image if needed (using high-quality smoothing)
      QImage scaledImage = region.image.scaled(displayRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

      // Draw the scaled image at the calculated position
      painter.drawImage(displayRect.topLeft(), scaledImage);
    }
  }

  // Draw border around subtitle area for debugging (only in verbose mode)
#ifdef DEBUG_SUBTITLES
  for (const SubtitleRegion& region : regionsCopy)
  {
    if (!region.image.isNull())
    {
      QRect displayRect = calculateDisplayRect(region.originalRect);

      painter.setPen(QPen(QColor(255, 255, 0), 2));        // Yellow border
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(displayRect);
    }
  }

  // Save rendered widget to file for debugging (only once per unique subtitle)
  static int saveCount = 0;
  static int lastImageHash = 0;

  if (!m_regions.isEmpty() &&
      (saveCount < 10))
  {
    // Calculate a simple hash of the subtitle to detect unique ones
    int currentHash = 0;

    for (const SubtitleRegion& region : m_regions)
    {
      currentHash += region.image.width() * 1000 + region.image.height() + region.originalRect.y();
    }

    if (currentHash != lastImageHash)
    {
      QString filename = QString("/tmp/subtitle_image_%1.png").arg(saveCount);

      // Save the first region as a sample
      if (!m_regions.first().image.isNull() &&
          m_regions.first().image.save(filename))
      {
        qDebug() << "Saved subtitle image to" << filename;
        saveCount++;
        lastImageHash = currentHash;
      }
    }
  }
#endif
}


void
SubtitleWidget::resizeEvent(QResizeEvent* event)
{
  // Recalculate scale factors based on new window size
  double windowAspect = (double)event->size().width() / event->size().height();
  double sourceAspect = (double)m_displayWidth / m_displayHeight;

  if (windowAspect > sourceAspect)
  {
    // Window is wider - scale based on height
    m_scaleY = (double)event->size().height() / m_displayHeight;
    m_scaleX = m_scaleY;
  }
  else
  {
    // Window is taller - scale based on width
    m_scaleX = (double)event->size().width() / m_displayWidth;
    m_scaleY = m_scaleX;
  }

  update();    // Repaint with new scale factor
}


QImage
SubtitleWidget::convertPixmapToImage(const LS_Pixmap_t* pixmap)
{
  if (!pixmap ||
      !pixmap->data ||
      (pixmap->width == 0) ||
      (pixmap->height == 0))
  {
    return QImage();
  }

  // Cast data to uint8_t* for access
  const uint8_t* data = static_cast<const uint8_t*>(pixmap->data);
  // Create image with ARGB32 format
  QImage image(pixmap->width, pixmap->height, QImage::Format_ARGB32);

  // Convert based on pixel format
  switch (pixmap->pixelFormat)
  {
    case LS_PIXFMT_ARGB32:
      // Direct copy for ARGB32
      memcpy(image.bits(), data, pixmap->width * pixmap->height * 4);
      break;

    case LS_PIXFMT_RGBA32:

      // Convert RGBA to ARGB (swap bytes)
      for (uint32_t y = 0; y < pixmap->height; y++)
      {
        for (uint32_t x = 0; x < pixmap->width; x++)
        {
          uint32_t i = y * pixmap->width + x;
          uint8_t r = data[i * 4 + 0];
          uint8_t g = data[i * 4 + 1];
          uint8_t b = data[i * 4 + 2];
          uint8_t a = data[i * 4 + 3];

          image.setPixel(x, y, QColor(r, g, b, a).rgba());
        }
      }

      break;

    case LS_PIXFMT_BGRA32:

      // Convert BGRA to ARGB
      for (uint32_t y = 0; y < pixmap->height; y++)
      {
        for (uint32_t x = 0; x < pixmap->width; x++)
        {
          uint32_t i = y * pixmap->width + x;
          uint8_t b = data[i * 4 + 0];
          uint8_t g = data[i * 4 + 1];
          uint8_t r = data[i * 4 + 2];
          uint8_t a = data[i * 4 + 3];

          image.setPixel(x, y, QColor(r, g, b, a).rgba());
        }
      }

      break;

    case LS_PIXFMT_PALETTE8BIT:

      // 8-bit palette
      for (uint32_t y = 0; y < pixmap->height; y++)
      {
        for (uint32_t x = 0; x < pixmap->width; x++)
        {
          uint8_t paletteIndex = data[y * pixmap->width + x];
          QColor color = getPixelColor(paletteIndex);

          image.setPixelColor(x, y, color);
        }
      }

      break;

    case LS_PIXFMT_PALETTE4BIT:

      // 4-bit palette (2 pixels per byte)
      for (uint32_t y = 0; y < pixmap->height; y++)
      {
        for (uint32_t x = 0; x < pixmap->width; x++)
        {
          uint32_t byteIndex = (y * pixmap->width + x) / 2;
          uint8_t byteValue = data[byteIndex];
          uint8_t paletteIndex;

          if (x % 2 == 0)
          {
            paletteIndex = (byteValue >> 4) & 0x0F;
          }
          else
          {
            paletteIndex = byteValue & 0x0F;
          }

          QColor color = getPixelColor(paletteIndex);

          image.setPixelColor(x, y, color);
        }
      }

      break;

    case LS_PIXFMT_PALETTE2BIT:

      // 2-bit palette (4 pixels per byte)
      for (uint32_t y = 0; y < pixmap->height; y++)
      {
        for (uint32_t x = 0; x < pixmap->width; x++)
        {
          uint32_t byteIndex = (y * pixmap->width + x) / 4;
          uint8_t byteValue = data[byteIndex];
          int shift = 6 - 2 * (x % 4);
          uint8_t paletteIndex = (byteValue >> shift) & 0x03;
          QColor color = getPixelColor(paletteIndex);

          image.setPixelColor(x, y, color);
        }
      }

      break;

    case LS_PIXFMT_YUV420:
      // YUV420 to RGB conversion
    {
      uint32_t ySize = pixmap->width * pixmap->height;
      uint32_t uvSize = ySize / 4;

      // Note: The pixmap data layout for YUV420 might be different
      // We'll try the standard planar format
      for (uint32_t y = 0; y < pixmap->height; y++)
      {
        for (uint32_t x = 0; x < pixmap->width; x++)
        {
          uint32_t pos = y * pixmap->width + x;
          uint8_t yVal = data[pos];
          uint8_t uVal = data[ySize + (y / 2) * (pixmap->width / 2) + (x / 2)];
          uint8_t vVal = data[ySize + uvSize + (y / 2) * (pixmap->width / 2) + (x / 2)];
          // YUV to RGB conversion (ITU-R BT.601)
          int c = yVal - 16;
          int d = uVal - 128;
          int e = vVal - 128;
          int r = (298 * c + 409 * e + 128) >> 8;
          int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
          int b = (298 * c + 516 * d + 128) >> 8;

          r = qBound(0, r, 255);
          g = qBound(0, g, 255);
          b = qBound(0, b, 255);
          image.setPixel(x, y, qRgb(r, g, b));
        }
      }
    }
    break;

    default:
      qWarning() << "Unsupported pixel format:" << pixmap->pixelFormat;
      return QImage();
  }

  return image;
}


QColor
SubtitleWidget::getPixelColor(uint8_t paletteIndex)
{
  // Default to transparent
  if (m_paletteData.isEmpty())
  {
    return QColor(0, 0, 0, 0);
  }

  // Get CLUT entry from stored palette data
  const LS_ColorRGB_t* clutEntry = reinterpret_cast<const LS_ColorRGB_t*>(m_paletteData.constData());
  uint8_t r = clutEntry[paletteIndex].redValue;
  uint8_t g = clutEntry[paletteIndex].greenValue;
  uint8_t b = clutEntry[paletteIndex].blueValue;
  uint8_t a = clutEntry[paletteIndex].alphaValue;

  return QColor(r, g, b, a);
}
