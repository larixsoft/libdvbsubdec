/*-----------------------------------------------------------------------------
 * DVB Subtitle Player - Subtitle Display Widget
 *---------------------------------------------------------------------------*/
/**
 * @file SubtitleWidget.h
 * @brief Subtitle rendering widget for DVB Subtitle Player
 *
 * @section overview Overview
 *
 * SubtitleWidget is a Qt widget that renders DVB subtitles. It receives
 * pixmap data from the decoder and displays them with proper scaling and
 * aspect ratio preservation.
 *
 * @section thread_safety Thread Safety
 *
 * All access to shared data (m_regions, m_paletteData) is protected by
 * m_mutex. This is necessary because:
 *
 * - Decoder callbacks (clearRegion, renderPixmap) run in main thread via QTimer
 * - paintEvent() runs in main thread via Qt event loop
 * - Both access m_regions concurrently
 *
 * Use QMutexLocker for exception-safe locking:
 *
 * @code
 * void clearRegion(int left, int top, int right, int bottom) {
 *     QMutexLocker locker(&m_mutex);
 *     // Modify m_regions safely
 * }
 * @endcode
 *
 * @section region_management Region Management
 *
 * Subtitles are composed of rectangular regions. The decoder:
 *
 * 1. Calls clearRegion() to clear areas (removes intersecting regions)
 * 2. Calls renderPixmap() to add new subtitle regions
 * 3. SubtitleWidget displays all active regions in paintEvent()
 *
 * @subsection clearing_region Clearing Regions
 *
 * When clearRegion() is called, all regions intersecting the clear rectangle
 * are removed:
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
 *     update();  // Trigger repaint
 * }
 * @endcode
 *
 * @subsection adding_regions Adding Regions
 *
 * New subtitle regions are added by renderPixmap():
 *
 * @code
 * void renderPixmap(const LS_Pixmap_t* pixmap, const uint8_t* palette) {
 *     QImage image = convertPixmapToImage(pixmap);
 *     QRect originalRect(pixmap->leftPos, pixmap->topPos,
 *                       pixmap->width, pixmap->height);
 *     SubtitleRegion region = {image, originalRect};
 *     QMutexLocker locker(&m_mutex);
 *     m_regions.append(region);
 *     update();  // Trigger repaint
 * }
 * @endcode
 *
 * @section pixel_format Pixel Format Handling
 *
 * The decoder provides pixmaps in various pixel formats:
 *
 * - LS_PIXFMT_ARGB32: 32-bit ARGB format (A-R-G-B byte order)
 * - LS_PIXFMT_PALETTE8BIT: 8-bit palette-indexed format
 *
 * For ARGB32, pixels are copied directly. For PALETTE8BIT, each palette
 * index is looked up in the CLUT to get the RGBA color.
 *
 * @code
 * QImage convertPixmapToImage(const LS_Pixmap_t* pixmap) {
 *     if (pixmap->pixelFormat == LS_PIXFMT_ARGB32) {
 *         // Direct copy of ARGB data
 *         return QImage((const uchar*)pixmap->data,
 *                      pixmap->width, pixmap->height,
 *                      QImage::Format_ARGB32);
 *     } else if (pixmap->pixelFormat == LS_PIXFMT_PALETTE8BIT) {
 *         // Convert palette indices to RGBA
 *         // ... (see implementation)
 *     }
 * }
 * @endcode
 *
 * @section display_scaling Display Scaling
 *
 * Subtitles are scaled to fit the display window while preserving aspect
 * ratio. The calculateDisplayRect() method computes the scaled position:
 *
 * @code
 * QRect calculateDisplayRect(const QRect& originalRect) const {
 *     double sourceAspect = (double)m_displayWidth / m_displayHeight;
 *     double windowAspect = (double)width() / height();
 *
 *     double scale, offsetX = 0, offsetY = 0;
 *     if (windowAspect > sourceAspect) {
 *         // Window wider - pillarbox
 *         scale = (double)height() / m_displayHeight;
 *         offsetX = (width() - m_displayWidth * scale) / 2.0;
 *     } else {
 *         // Window taller - letterbox
 *         scale = (double)width() / m_displayWidth;
 *         offsetY = (height() - m_displayHeight * scale) / 2.0;
 *     }
 *
 *     return QRect(
 *         (int)(originalRect.x() * scale + offsetX),
 *         (int)(originalRect.y() * scale + offsetY),
 *         (int)(originalRect.width() * scale),
 *         (int)(originalRect.height() * scale)
 *     );
 * }
 * @endcode
 *
 * @section rendering Rendering Pipeline
 *
 * 1. Decoder calls clearRegion() - removes intersecting regions
 * 2. Decoder calls renderPixmap() - adds new subtitle region
 * 3. Qt calls paintEvent() - renders all regions
 *
 * @code
 * void paintEvent(QPaintEvent* event) {
 *     QPainter painter(this);
 *     painter.fillRect(rect(), Qt::black);  // Background
 *
 *     QMutexLocker locker(&m_mutex);
 *     for (const SubtitleRegion& region : m_regions) {
 *         QRect displayRect = calculateDisplayRect(region.originalRect);
 *         painter.drawImage(displayRect, region.image);
 *     }
 * }
 * @endcode
 */

#ifndef SUBTITLEWIDGET_H
#define SUBTITLEWIDGET_H

#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QResizeEvent>
#include <QVector>
#include <QMutex>
#include "lssubdec.h"
// Structure to hold a single subtitle region
struct SubtitleRegion
{
  QImage image;
  QRect  originalRect;      // Original coordinates from decoder (for clearRegion matching)
};


class SubtitleWidget : public QWidget
{
  Q_OBJECT

public:
  explicit SubtitleWidget(QWidget* parent = nullptr);
  ~SubtitleWidget();
  void setDisplaySize(int width, int height);
  void clear();
  void clearRegion(int left, int top, int right, int bottom); // Clear specific region
  void renderPixmap(const LS_Pixmap_t* pixmap, const uint8_t* palette = nullptr);
  void startNewSubtitle(int subtitleCount);                   // Start a new subtitle page
  void finishSubtitle();                                      // Finish rendering current subtitle and trigger update

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  QImage convertPixmapToImage(const LS_Pixmap_t* pixmap);
  QColor getPixelColor(uint8_t paletteIndex);
  QRect  calculateDisplayRect(const QRect& originalRect) const; // Calculate scaled display position
  void   updateDisplaySize(const QRect& regionRect);            // Auto-detect display size from region

  QMutex                  m_mutex;                              // Protects m_regions and m_paletteData
  QVector<SubtitleRegion> m_regions;                            // All regions for current subtitle
  int                     m_displayWidth;
  int                     m_displayHeight;
  QByteArray              m_paletteData;                        // Store CLUT palette data
  double                  m_scaleX;                             // Horizontal scale factor (window_width / source_width)
  double                  m_scaleY;                             // Vertical scale factor (window_height / source_height)
  // Default DVB resolution (used if no DDS is present)
  static const int        DEFAULT_DVB_WIDTH = 720;
  static const int        DEFAULT_DVB_HEIGHT = 576;
};

#endif // SUBTITLEWIDGET_H
