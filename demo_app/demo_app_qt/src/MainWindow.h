/*-----------------------------------------------------------------------------
 * DVB Subtitle Player - Main Window
 *---------------------------------------------------------------------------*/
/**
 * @file MainWindow.h
 * @brief Main window for DVB Subtitle Player Qt application
 *
 * @section overview Overview
 *
 * MainWindow is a simple DVB subtitle player that:
 * - Loads PES files containing DVB subtitle streams
 * - Processes all PES packets automatically on file load
 * - Renders subtitles to the display widget
 * - Shows basic statistics (frames, subtitle count, progress)
 *
 * @section usage Usage
 *
 * The player automatically processes the entire PES stream when a file is loaded:
 * - Launch with a PES file: dvbplayer <path/to/file.pes>
 * - Or use the Open button to load a file via the file dialog
 * - Subtitles are displayed incrementally as they are decoded
 *
 * @section decoder_integration Decoder Integration
 *
 * The decoder is integrated through static callback functions that are
 * registered with the decoder service. These callbacks are invoked by the
 * decoder and must be thread-safe.
 *
 * @section pes_processing PES Processing
 *
 * PES (Packetized Elementary Stream) files contain subtitle data packets.
 * Each packet starts with the start code 0x000001 followed by a stream ID:
 * - 0xBD: Private Stream 1 (DVB subtitles)
 * - 0xBE: Padding stream (may contain subtitles)
 *
 * The application processes PES packets incrementally using a QTimer:
 * 1. Scan for PES packet start codes
 * 2. Extract packet size from PES header
 * 3. Feed packet to decoder via LS_DVBSubDecServicePlay()
 * 4. Decoder invokes callbacks to update display
 * 5. Repeat until all packets processed
 *
 * @section dds_handling Display Definition Segment (DDS)
 *
 * The DDS provides the original video dimensions for the subtitle stream.
 * When received:
 * 1. Update m_ddsDisplayWidth and m_ddsDisplayHeight
 * 2. Recalculate subtitle scaling
 * 3. Enable letterbox/pillarbox for aspect ratio preservation
 *
 * @section ui_controls UI Controls
 *
 * - Open Button: Open new PES file
 * - Time Label: Display current position (calculated from frame count)
 * - Stats Label: Display decoder statistics (frames, subtitle count)
 * - Progress Bar: Show playback progress (100% when complete)
 *
 * @section keyboard_shortcuts Keyboard Shortcuts
 *
 * - O: Open file
 * - Q: Quit application
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include "lssubdec.h"

class SubtitleWidget;
class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(const QString& pesFile, QWidget* parent = nullptr);
  ~MainWindow();
  void setDisplaySize(int width, int height);
  void setVerbose(bool verbose);
  void setLoop(bool loop);
  void setPlaybackSpeed(double speed);

protected:
  void closeEvent(QCloseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

private slots:
  void openFile();
  void processNextPESPacket();    // Process one PES packet at a time

private:
  void      setupUi();
  void      setupDecoder();
  void      cleanupDecoder();
  void      loadPESFile(const QString& filename);
  void      updateStatusBar();
  LS_Status processAllPESData();

  // OSD callbacks (static wrappers)
  static LS_Status cleanRegionCallback(LS_Rect_t rect, void* userData);
  static LS_Status drawPixmapCallback(const LS_Pixmap_t* pixmap,
                                      const uint8_t*     palette,
                                      const uint8_t      paletteNum,
                                      void*              userData);
  static LS_Status ddsNotifyCallback(uint16_t displayWidth, uint16_t displayHeight, void* userData);
  static LS_Status getCurrentPCRCallback(uint64_t* currentPCR, void* userData);

  // Member variables
  QString         m_pesFile;
  SubtitleWidget* m_subtitleWidget;
  // Decoder state
  LS_ServiceID_t  m_serviceId;
  uint8_t*        m_pesBuffer;
  size_t          m_pesBufferSize;
  size_t          m_pesCurrentPos;
  size_t          m_pesProcessingPos;   // Position for incremental PES packet processing
  int             m_pesPacketCount;     // Count of processed packets
  bool            m_decoderCleanupDone; // Flag to prevent double cleanup
  // Playback state
  bool            m_isLooping;
  int             m_displayWidth;
  int             m_displayHeight;
  bool            m_verbose;
  // DDS display dimensions (from Display Definition Segment)
  uint16_t        m_ddsDisplayWidth;
  uint16_t        m_ddsDisplayHeight;
  bool            m_ddsReceived;
  uint32_t        m_frameCount;
  uint32_t        m_subtitleCount;
  uint64_t        m_currentPCR;                 // Current PCR value (90kHz clock)
  int             m_lastRenderedY;              // Track last rendered Y position to detect new subtitle pages
  bool            m_dataProcessed;              // Flag to track if data has been fed to decoder
  // UI elements
  QPushButton*    m_openButton;
  QLabel*         m_timeLabel;
  QLabel*         m_statsLabel;
  QProgressBar*   m_progressBar;
  QTimer*         m_pesProcessingTimer; // Timer for incremental PES packet processing

  enum { MAX_PES_BUFFER = 500 * 1024 * 1024 };
};

#endif // MAINWINDOW_H
