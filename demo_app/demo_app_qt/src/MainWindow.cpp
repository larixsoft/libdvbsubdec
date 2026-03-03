/*-----------------------------------------------------------------------------
 * DVB Subtitle Player - Main Window Implementation
 *---------------------------------------------------------------------------*/
/**
 * @file MainWindow.cpp
 * @brief Implementation of MainWindow for DVB Subtitle Player
 *
 * @section timer_system Qt-based Timer System
 *
 * The decoder library requires timer functionality. This file provides
 * a Qt-based timer implementation that is thread-safe through Qt's event loop:
 *
 * @code
 * // Timer creation - wraps QTimer in TimerInfo struct
 * LS_Timer_t timer_id;
 * LS_TimerCreate(&timer_id, callback, param);
 *
 * // When timer fires, Qt event loop invokes callback in main thread
 * // This is SAFE because QTimer callbacks run in the thread that created them
 * @endcode
 *
 * @section pthread_mutex pthread Mutex Functions
 *
 * The decoder library requires mutex functionality. On Linux, we use
 * POSIX pthread mutexes:
 *
 * @code
 * pthread_mutex_create: Allocate and initialize pthread_mutex_t
 * pthread_mutex_delete: Destroy and free pthread_mutex_t
 * pthread_mutex_wait: Lock mutex (pthread_mutex_lock)
 * pthread_mutex_signal: Unlock mutex (pthread_mutex_unlock)
 * @endcode
 *
 * @section callback_implementation Decoder Callback Implementation
 *
 * The decoder invokes callbacks during LS_DVBSubDecServicePlay():
 *
 * @subsection clean_region Clean Region Callback
 *
 * Called when the decoder wants to clear a rectangular area:
 *
 * @code
 * LS_Status MainWindow::cleanRegionCallback(LS_Rect_t rect, void* userData)
 * {
 *     MainWindow* window = static_cast<MainWindow*>(userData);
 *     window->m_subtitleWidget->clearRegion(rect.leftPos, rect.topPos,
 *                                         rect.rightPos, rect.bottomPos);
 *     return LS_OK;
 * }
 * @endcode
 *
 * @subsection draw_pixmap Draw Pixmap Callback
 *
 * Called when the decoder has a subtitle pixmap to display:
 *
 * @code
 * LS_Status MainWindow::drawPixmapCallback(const LS_Pixmap_t* pixmap,
 *                                          const uint8_t* palette,
 *                                          uint8_t paletteNum, void* userData)
 * {
 *     MainWindow* window = static_cast<MainWindow*>(userData);
 *     window->m_subtitleWidget->addPixmap(pixmap, palette);
 *     return LS_OK;
 * }
 * @endcode
 *
 * @subsection dds_notify DDS Notify Callback
 *
 * Called when Display Definition Segment is received:
 *
 * @code
 * LS_Status MainWindow::ddsNotifyCallback(uint16_t displayWidth,
 *                                         uint16_t displayHeight,
 *                                         void* userData)
 * {
 *     MainWindow* window = static_cast<MainWindow*>(userData);
 *     window->m_ddsDisplayWidth = displayWidth;
 *     window->m_ddsDisplayHeight = displayHeight;
 *     window->m_ddsReceived = true;
 *     return LS_OK;
 * }
 * @endcode
 *
 * @subsection get_pcr Get PCR Callback
 *
 * Called to get current PCR timestamp:
 *
 * @code
 * LS_Status MainWindow::getCurrentPCRCallback(uint64_t* currentPCR, void* userData)
 * {
 *     MainWindow* window = static_cast<MainWindow*>(userData);
 *     *currentPCR = window->m_currentPCR;
 *     return LS_OK;
 * }
 * @endcode
 *
 * @section pes_processing PES Processing Implementation
 *
 * PES packets are processed incrementally to allow real-time display:
 *
 * @code
 * void MainWindow::processNextPESPacket()
 * {
 *     // Find next PES packet start code (00 00 01)
 *     size_t packetStart = findNextPESPacket(m_pesCurrentPos);
 *     if (packetStart >= m_pesBufferSize) {
 *         // End of file - stop or loop
 *         if (m_isLooping) {
 *             m_pesCurrentPos = 0;
 *         } else {
 *             m_playbackTimer->stop();
 *         }
 *         return;
 *     }
 *
 *     // Get packet size from PES header
 *     size_t packetSize = getPESPacketSize(packetStart);
 *
 *     // Feed to decoder
 *     LS_CodedData_t pesData = {
 *         .data = m_pesBuffer + packetStart,
 *         .dataSize = packetSize
 *     };
 *     LS_PageId_t pageId = {0, 0};  // Wildcard
 *     LS_DVBSubDecServicePlay(m_serviceId, &pesData, pageId);
 *
 *     // Move to next packet
 *     m_pesCurrentPos = packetStart + packetSize;
 * }
 * @endcode
 *
 * @section display_scaling Display Scaling
 *
 * Subtitle coordinates are scaled to fit the display window while
 * preserving aspect ratio (letterbox/pillarbox):
 *
 * @code
 * QRect MainWindow::calculateDisplayRect(const QRect& originalRect)
 * {
 *     // Get source and target aspect ratios
 *     double sourceAspect = (double)m_ddsDisplayWidth / m_ddsDisplayHeight;
 *     double windowAspect = (double)width() / height();
 *
 *     double scale, offsetX = 0, offsetY = 0;
 *     if (windowAspect > sourceAspect) {
 *         // Window wider - pillarbox
 *         scale = (double)height() / m_ddsDisplayHeight;
 *         offsetX = (width() - m_ddsDisplayWidth * scale) / 2.0;
 *     } else {
 *         // Window taller - letterbox
 *         scale = (double)width() / m_ddsDisplayWidth;
 *         offsetY = (height() - m_ddsDisplayHeight * scale) / 2.0;
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
 * @section cleanup_proper Proper Cleanup Sequence
 *
 * CRITICAL: Cleanup must happen in this order to prevent crashes:
 *
 * @code
 * MainWindow::~MainWindow()
 * {
 *     // 1. Stop all timers FIRST (prevents callbacks during cleanup)
 *     if (m_playbackTimer) {
 *         m_playbackTimer->stop();
 *     }
 *     if (m_pesProcessingTimer) {
 *         m_pesProcessingTimer->stop();
 *     }
 *
 *     // 2. Clear decoder cleanup flag
 *     m_decoderCleanupDone = false;
 *
 *     // 3. Cleanup decoder service
 *     cleanupDecoder();
 *
 *     // 4. Free PES buffer
 *     if (m_pesBuffer) {
 *         free(m_pesBuffer);
 *         m_pesBuffer = nullptr;
 *     }
 *
 *     // 5. Cleanup timer system
 *     cleanupTimerSystem();
 * }
 * @endcode
 */

#include "MainWindow.h"
#include "SubtitleWidget.h"
#include "lsptsmgr.h"  // For LS_PTSMgrGetInitialPCR
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QMessageBox>
#include <QKeyEvent>
#include <QDebug>
#include <QTimer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QByteArray>
#include <QList>
#include <cstring>
#include <pthread.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unordered_map>
/*-----------------------------------------------------------------------------
 * Timer support
 *---------------------------------------------------------------------------*/
struct TimerInfo
{
  void    (*callback)(void* param);
  void*   param;
  QTimer* qtimer;
  int     timerId; // For debugging
};


static std::unordered_map<LS_Timer_t, TimerInfo*> g_timers;
static uint64_t g_nextTimerId = 1;
static QObject* g_timerParent = nullptr;  // Parent for all timers - must be in main thread

// Initialize timer parent - must be called from main thread
static void
initTimerSystem(QObject* parent)
{
  if (!g_timerParent)
  {
    g_timerParent = new QObject(parent);
    g_timerParent->setObjectName("TimerParent");
    // Timer system initialized
  }
}


static void
cleanupTimerSystem()
{
  if (g_timerParent)
  {
    // Stop all active timers
    for (auto& pair : g_timers)
    {
      TimerInfo* info = pair.second;

      if (info &&
          info->qtimer)
      {
        if (info->qtimer->isActive())
        {
          info->qtimer->stop();
        }

        delete info->qtimer;
      }

      delete info;
    }

    g_timers.clear();
    // Delete the timer parent
    delete g_timerParent;
    g_timerParent = nullptr;
    g_nextTimerId = 0;
    // Timer system cleaned up
  }
}


/*-----------------------------------------------------------------------------
 * pthread mutex functions for Linux
 *---------------------------------------------------------------------------*/
static int32_t
pthread_mutex_create(LS_Mutex_t* mutex)
{
  pthread_mutex_t* m = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));

  if (!m)
  {
    return LS_ERROR_SYSTEM_ERROR;
  }

  if (pthread_mutex_init(m, NULL) != 0)
  {
    free(m);
    return LS_ERROR_SYSTEM_ERROR;
  }

  *mutex = m;
  return LS_OK;
}


static int32_t
pthread_mutex_delete(LS_Mutex_t mutex)
{
  if (!mutex)
  {
    return LS_ERROR_GENERAL;
  }

  pthread_mutex_t* m = (pthread_mutex_t*)mutex;

  pthread_mutex_destroy(m);
  free(m);
  return LS_OK;
}


static int32_t
pthread_mutex_wait(LS_Mutex_t mutex)
{
  if (!mutex)
  {
    return LS_ERROR_GENERAL;
  }

  pthread_mutex_t* m = (pthread_mutex_t*)mutex;

  return (pthread_mutex_lock(m) == 0) ? LS_OK : LS_ERROR_SYSTEM_ERROR;
}


static int32_t
pthread_mutex_signal(LS_Mutex_t mutex)
{
  if (!mutex)
  {
    return LS_ERROR_GENERAL;
  }

  pthread_mutex_t* m = (pthread_mutex_t*)mutex;

  return (pthread_mutex_unlock(m) == 0) ? LS_OK : LS_ERROR_SYSTEM_ERROR;
}


/*-----------------------------------------------------------------------------
 * Qt timer functions for DVB Subtitle Decoder
 *---------------------------------------------------------------------------*/
static int32_t
qtimer_create(LS_Timer_t* timer_id, void (*callback_func) (void*), void* param)
{
  if (!timer_id ||
      !callback_func)
  {
    return LS_ERROR_GENERAL;
  }

  if (!g_timerParent)
  {
    return LS_ERROR_GENERAL;
  }

  TimerInfo* info = new TimerInfo();

  info->callback = callback_func;
  info->param = param;
  info->timerId = static_cast<int>(g_nextTimerId);
  // Create timer with g_timerParent as parent - ensures correct thread affinity
  info->qtimer = new QTimer(g_timerParent);
  info->qtimer->setSingleShot(true);

  // Use the timer's address as the ID (we store the pointer in the map)
  LS_Timer_t id = reinterpret_cast<LS_Timer_t>(g_nextTimerId++);

  g_timers[id] = info;
  // Connect the timer's timeout signal to a lambda that calls the callback
  // Use g_timerParent as context object to ensure lambda runs in main thread
  QObject::connect(info->qtimer,
                   &QTimer::timeout,
                   g_timerParent,
                   [info] ()
  {
    if (info->callback)
    {
      info->callback(info->param);
    }
  },
                   Qt::QueuedConnection);
  *timer_id = id;
  return LS_OK;
}


static int32_t
qtimer_delete(LS_Timer_t timer_id)
{
  auto it = g_timers.find(timer_id);

  if (it == g_timers.end())
  {
    return LS_ERROR_GENERAL;
  }

  TimerInfo* info = it->second;

  if (info->qtimer)
  {
    info->qtimer->stop();
    delete info->qtimer;
  }

  delete info;
  g_timers.erase(it);
  return LS_OK;
}


static int32_t
qtimer_start(LS_Timer_t timer_id, uint32_t time_in_millisec)
{
  auto it = g_timers.find(timer_id);

  if (it == g_timers.end())
  {
    return LS_ERROR_GENERAL;
  }

  TimerInfo* info = it->second;

  if (!info->qtimer)
  {
    return LS_ERROR_GENERAL;
  }

  info->qtimer->start(time_in_millisec);
  return LS_OK;
}


static int32_t
qtimer_stop(LS_Timer_t timer_id, LS_Time_t* time_left)
{
  auto it = g_timers.find(timer_id);

  if (it == g_timers.end())
  {
    return LS_ERROR_GENERAL;
  }

  TimerInfo* info = it->second;

  if (!info->qtimer)
  {
    return LS_ERROR_GENERAL;
  }

  if (info->qtimer->isActive())
  {
    int remaining = info->qtimer->remainingTime();

    info->qtimer->stop();

    if (time_left)
    {
      time_left->seconds = remaining / 1000;
      time_left->milliseconds = remaining % 1000;
    }
  }

  return LS_OK;
}


MainWindow::MainWindow(const QString& pesFile, QWidget* parent)
  : QMainWindow(parent)
  , m_pesFile(pesFile)
  , m_subtitleWidget(nullptr)
  , m_serviceId(nullptr)
  , m_pesBuffer(nullptr)
  , m_pesBufferSize(0)
  , m_pesCurrentPos(0)
  , m_pesProcessingPos(0)
  , m_pesPacketCount(0)
  , m_decoderCleanupDone(false)
  , m_isLooping(false)
  , m_displayWidth(720)
  , m_displayHeight(576)
  , m_verbose(false)
  , m_ddsDisplayWidth(0)
  , m_ddsDisplayHeight(0)
  , m_ddsReceived(false)
  , m_frameCount(0)
  , m_subtitleCount(0)
  , m_currentPCR(0)
  , m_lastRenderedY(-1)
  , m_dataProcessed(false)
  , m_openButton(nullptr)
  , m_timeLabel(nullptr)
  , m_statsLabel(nullptr)
  , m_progressBar(nullptr)
  , m_pesProcessingTimer(nullptr)
{
  // Initialize timer system FIRST - must be done in main thread before decoder uses timers
  initTimerSystem(this);
  setupUi();
  setupDecoder();

  // Load initial file if specified
  if (!m_pesFile.isEmpty())
  {
    loadPESFile(m_pesFile);
  }
}


MainWindow::~MainWindow()
{
  cleanupDecoder();

  if (m_pesBuffer)
  {
    delete[] m_pesBuffer;
  }
}


void
MainWindow::setupUi()
{
  setWindowTitle("DVB Subtitle Player");
  resize(900, 700);
  // Create central widget with subtitle display
  m_subtitleWidget = new SubtitleWidget(this);
  m_subtitleWidget->setMinimumSize(720, 576);
  m_subtitleWidget->setStyleSheet("background-color: black;");
  setCentralWidget(m_subtitleWidget);

  // Create toolbar
  QToolBar* toolbar = addToolBar("Playback");

  toolbar->setMovable(false);
  // Open button
  m_openButton = new QPushButton("Open", this);
  m_openButton->setToolTip("Open PES file");
  connect(m_openButton, &QPushButton::clicked, this, &MainWindow::openFile);
  toolbar->addWidget(m_openButton);
  // Status bar
  m_timeLabel = new QLabel("00:00", this);
  m_statsLabel = new QLabel("Frames: 0 | Subtitles: 0", this);
  m_progressBar = new QProgressBar(this);
  m_progressBar->setRange(0, 100);
  m_progressBar->setValue(0);
  statusBar()->addWidget(m_timeLabel);
  statusBar()->addWidget(m_statsLabel);
  statusBar()->addPermanentWidget(m_progressBar);
  // PES processing timer (for incremental processing to avoid blocking UI)
  m_pesProcessingTimer = new QTimer(this);
  m_pesProcessingTimer->setSingleShot(false);
  connect(m_pesProcessingTimer, &QTimer::timeout, this, &MainWindow::processNextPESPacket);
  updateStatusBar();
}


void
MainWindow::setDisplaySize(int width, int height)
{
  m_displayWidth = width;
  m_displayHeight = height;
  m_subtitleWidget->setDisplaySize(width, height);
}


void
MainWindow::setVerbose(bool verbose)
{
  m_verbose = verbose;
}


void
MainWindow::setLoop(bool loop)
{
  m_isLooping = loop;
}


void
MainWindow::setPlaybackSpeed(double speed)
{
  // Playback speed control removed - file auto-plays on load
  (void)speed;
}


/*-----------------------------------------------------------------------------
 * Timestamp function for decoder logging
 *---------------------------------------------------------------------------*/
static char timestamp_buffer[32];

static char*
get_timestamp_func(void)
{
  // Get current time in milliseconds since application start
  static QElapsedTimer timer;

  if (!timer.isValid())
  {
    timer.start();
  }

  qint64 elapsed = timer.elapsed();
  qint64 seconds = elapsed / 1000;
  qint64 millis = elapsed % 1000;

  // Format: sssss.mmm (seconds.milliseconds)
  snprintf(timestamp_buffer, sizeof(timestamp_buffer), "%05lld.%03lld", seconds, millis);
  return timestamp_buffer;
}


void
MainWindow::setupDecoder()
{
  // Initialize system functions with pthread mutex functions and Qt timer functions
  LS_SystemFuncs_t sysFuncs;

  memset(&sysFuncs, 0, sizeof(sysFuncs));
  sysFuncs.mutexCreateFunc = pthread_mutex_create;
  sysFuncs.mutexDeleteFunc = pthread_mutex_delete;
  sysFuncs.mutexWaitFunc = pthread_mutex_wait;
  sysFuncs.mutexSignalFunc = pthread_mutex_signal;
  sysFuncs.timerCreateFunc = qtimer_create;
  sysFuncs.timerDeleteFunc = qtimer_delete;
  sysFuncs.timerStartFunc = qtimer_start;
  sysFuncs.timerStopFunc = qtimer_stop;
  sysFuncs.getTimeStampFunc = get_timestamp_func;    // Add timestamp function

  LS_Status status = LS_DVBSubDecInit(1024 * 1024, sysFuncs);

  if (status != LS_OK)
  {
    QMessageBox::critical(this, "Error", QString("Failed to initialize decoder library: %1").arg(status));
    return;
  }

  LS_ServiceMemCfg_t memCfg;

  memCfg.codedDataBufferSize = 512 * 1024;
  memCfg.pixelBufferSize = 16 * 1024 * 1024;      // Increased to 16MB for multiple display sets
  memCfg.compositionBufferSize = 2 * 1024 * 1024; // Increased to 2MB for streams with many displaysets
  m_serviceId = LS_DVBSubDecServiceNew(memCfg);

  if (!m_serviceId)
  {
    QMessageBox::critical(this, "Error", "Failed to create subtitle service");
    LS_DVBSubDecFinalize();
    return;
  }

  // Setup OSD rendering with our callbacks
  LS_OSDRender_t osdRender;

  memset(&osdRender, 0, sizeof(osdRender));
  osdRender.cleanRegionFunc = cleanRegionCallback;
  osdRender.drawPixmapFunc = drawPixmapCallback;
  osdRender.ddsNotifyFunc = ddsNotifyCallback;
  osdRender.getCurrentPCRFunc = getCurrentPCRCallback;
  osdRender.cleanRegionFuncData = this;
  osdRender.drawPixmapFuncData = this;
  osdRender.ddsNotifyFuncData = this;
  osdRender.getCurrentPCRFuncData = this;
  osdRender.OSDPixmapFormat = LS_PIXFMT_ARGB32;
  osdRender.alphaValueFullOpaque = 255;
  osdRender.alphaValueFullTransparent = 0;
  /* NOTE: PTS sync disabled for demo - would require actual video playback timing */
  osdRender.ptsSyncEnabled = 0;           // Disable PTS sync (no video timeline in demo)
  osdRender.ptsSyncTolerance = 100;       // Max late tolerance: 100ms
  osdRender.ptsMaxEarlyDisplay = 50;      // Max early display: 50ms
  osdRender.backgroundColor.colorData.rgbColor.redValue = 0;
  osdRender.backgroundColor.colorData.rgbColor.greenValue = 0;
  osdRender.backgroundColor.colorData.rgbColor.blueValue = 0;
  osdRender.backgroundColor.colorData.rgbColor.alphaValue = 0;
  status = LS_DVBSubDecServiceStart(m_serviceId, osdRender);

  if (status != LS_OK)
  {
    QMessageBox::critical(this, "Error", QString("Failed to start service: %1").arg(status));
  }
}


void
MainWindow::cleanupDecoder()
{
  if (m_decoderCleanupDone)
  {
    return;      // Already cleaned up, prevent double cleanup
  }

  // Stop playback first to prevent timer callbacks
  if (m_pesProcessingTimer)
  {
    m_pesProcessingTimer->stop();
  }

  // Give timers time to finish any pending callbacks
  QCoreApplication::processEvents();

  // Cleanup decoder service
  if (m_serviceId)
  {
    LS_DVBSubDecServiceStop(m_serviceId);
    LS_DVBSubDecServiceDelete(m_serviceId);
    m_serviceId = nullptr;
  }

  // Finalize decoder library (this also finalizes PTS manager)
  LS_DVBSubDecFinalize();
  // Cleanup timer system
  cleanupTimerSystem();
  m_decoderCleanupDone = true;
}


void
MainWindow::loadPESFile(const QString& filename)
{
  // Clean up previous buffer
  if (m_pesBuffer)
  {
    delete[] m_pesBuffer;
    m_pesBuffer = nullptr;
  }

  // Load file
  QFile file(filename);

  if (!file.open(QIODevice::ReadOnly))
  {
    QMessageBox::critical(this, "Error", "Cannot open file: " + filename);
    return;
  }

  m_pesBufferSize = file.size();

  if (m_pesBufferSize > MAX_PES_BUFFER)
  {
    QMessageBox::warning(this,
                         "Warning",
                         QString("File is large (%1 MB), truncating to %2 MB").arg(m_pesBufferSize /
                                                                                   (1024 * 1024)).arg(MAX_PES_BUFFER /
                                                                                                      (1024 * 1024)));
    m_pesBufferSize = MAX_PES_BUFFER;
  }

  m_pesBuffer = new uint8_t[m_pesBufferSize];

  size_t bytesRead = file.read((char*)m_pesBuffer, m_pesBufferSize);

  if (bytesRead != m_pesBufferSize)
  {
    QMessageBox::critical(this, "Error", "Failed to read file");
    delete[] m_pesBuffer;
    m_pesBuffer = nullptr;
    return;
  }

  m_pesCurrentPos = 0;
  m_frameCount = 0;
  m_currentPCR = 0;
  m_subtitleCount = 0;
  m_dataProcessed = false;    // Reset for new file
  m_pesFile = filename;
  setWindowTitle(QString("DVB Subtitle Player - %1").arg(QFileInfo(filename).fileName()));
  // Reset display
  m_subtitleWidget->clear();
  m_progressBar->setValue(0);
  updateStatusBar();

  // Process all PES data immediately on file load
  if (processAllPESData() == LS_OK)
  {
    m_dataProcessed = true;
    m_progressBar->setValue(100);
    updateStatusBar();
  }
}


void
MainWindow::openFile()
{
  QString filename = QFileDialog::getOpenFileName(this, "Open PES File", "", "PES Files (*.pes);;All Files (*)");

  if (!filename.isEmpty())
  {
    loadPESFile(filename);
  }
}


LS_Status
MainWindow::processAllPESData()
{
  if (!m_pesBuffer ||
      (m_pesBufferSize == 0))
  {
    return LS_ERROR_GENERAL;
  }

  m_dataProcessed = true;
  // Reset processing state
  m_pesProcessingPos = 0;
  m_pesPacketCount = 0;

  // Start incremental processing - process one packet per timer tick
  // Using 50ms interval to ensure PTS timers have time to fire and free memory
  if (m_pesProcessingTimer)
  {
    m_pesProcessingTimer->start(50);      // 20 packets per second, allows PTS callbacks to fire
  }

  return LS_OK;
}


void
MainWindow::processNextPESPacket()
{
  static QElapsedTimer startTime;

  if (!startTime.isValid())
  {
    startTime.start();
  }

  (void)startTime.elapsed();  // For timing information
  LS_PageId_t pageId;

  pageId.compositionPageId = 0;    // 0 = wildcard (accept any page_id)
  pageId.ancillaryPageId = 0;      // 0 = wildcard (accept any page_id)

  // Process one PES packet per call
  while (m_pesProcessingPos + 6 < m_pesBufferSize)
  {
    // Find next PES packet start (00 00 01 BD or BE)
    while (m_pesProcessingPos + 6 < m_pesBufferSize)
    {
      if ((m_pesBuffer[m_pesProcessingPos] == 0x00) &&
          (m_pesBuffer[m_pesProcessingPos + 1] == 0x00) &&
          (m_pesBuffer[m_pesProcessingPos + 2] == 0x01) &&
          ((m_pesBuffer[m_pesProcessingPos + 3] == 0xBD) ||
           (m_pesBuffer[m_pesProcessingPos + 3] == 0xBE)))
      {
        break;
      }

      m_pesProcessingPos++;
    }

    if (m_pesProcessingPos + 6 >= m_pesBufferSize)
    {
      break;
    }

    // Extract PES packet length (big-endian at offset 4)
    uint16_t pesLen = (m_pesBuffer[m_pesProcessingPos + 4] << 8) | m_pesBuffer[m_pesProcessingPos + 5];
    size_t packetEnd = m_pesProcessingPos + 6 + pesLen;

    if (packetEnd > m_pesBufferSize)
    {
      packetEnd = m_pesBufferSize;
    }

    // Only process stream_id 0xBD (Private Stream 1 - subtitles)
    if (m_pesBuffer[m_pesProcessingPos + 3] == 0xBD)
    {
      LS_CodedData_t pesData;

      pesData.data = m_pesBuffer + m_pesProcessingPos;
      pesData.dataSize = packetEnd - m_pesProcessingPos;

      LS_Status status = LS_DVBSubDecServicePlay(m_serviceId, &pesData, pageId);

      if (status != LS_OK)
      {
        // Silent handling of failed packets
      }

      m_pesPacketCount++;
      // Move to next packet and return - let the timer call us again
      // This allows the UI to update between packets
      m_pesProcessingPos = packetEnd;
      // Note: Don't call processEvents() here - it can cause re-entrancy issues
      // The timer interval (20ms) provides enough time for the event loop to run naturally
      return;
    }

    m_pesProcessingPos = packetEnd;
  }

  // All packets processed - stop the timer
  printf("[%-5lldms] PES processing complete: %d packets processed\n", startTime.elapsed(), m_pesPacketCount);
  fflush(stdout);

  if (m_pesProcessingTimer)
  {
    m_pesProcessingTimer->stop();
  }
}


void
MainWindow::updateStatusBar()
{
  // Calculate time (assuming 25 fps)
  int seconds = m_frameCount / 25;
  int minutes = seconds / 60;

  seconds %= 60;
  m_timeLabel->setText(QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0')));
  m_statsLabel->setText(QString("Frames: %1 | Subtitles: %2").arg(m_frameCount).arg(m_subtitleCount));
}


LS_Status
MainWindow::cleanRegionCallback(LS_Rect_t rect, void* userData)
{
  MainWindow* window = static_cast<MainWindow*>(userData);

  if (!window)
  {
    return LS_ERROR_GENERAL;
  }

  // Detect new subtitle by tracking clean cycles
  // Each subtitle has 2 regions at different Y positions (e.g., 790 and 872)
  // We detect a new subtitle when we see a "reset" to a lower Y value
  static int lastClearedY = -1;
  bool isNewSubtitle = false;

  if (lastClearedY == -1)
  {
    // First clean region ever - this is clearing for subtitle #2 (subtitle #1 had no clear)
    isNewSubtitle = true;
  }
  else if (rect.topPos < lastClearedY)
  {
    // Y position reset to a lower value - new subtitle starting
    // Example: previous Y was 872, now Y is 790 (lower), so new subtitle
    isNewSubtitle = true;
  }

  lastClearedY = rect.topPos;

  if (isNewSubtitle)
  {
    window->m_subtitleCount++;
    window->updateStatusBar();
  }

  // Clear only the specific region the decoder asked us to clear
  window->m_subtitleWidget->clearRegion(rect.leftPos, rect.topPos, rect.rightPos, rect.bottomPos);
  return LS_OK;
}


// Helper structure to pass pixmap data for deferred rendering
struct DeferredRenderData
{
  LS_Pixmap_t pixmap;
  QByteArray  paletteData;
  int         regionCount;
  int         subtitleCount;
  qint64      timestamp;
  MainWindow* window;
  bool        verbose;
  DeferredRenderData()
    : pixmap{}
    , regionCount(0)
    , subtitleCount(0)
    , timestamp(0)
    , window(nullptr)
    , verbose(false)
  {
    pixmap.data = nullptr;
  }


  ~DeferredRenderData()
  {
    if (pixmap.data)
    {
      free(const_cast<void*>(pixmap.data));
    }
  }
};


LS_Status
MainWindow::drawPixmapCallback(const LS_Pixmap_t* pixmap,
                               const uint8_t*     palette,
                               const uint8_t      paletteNum,
                               void*              userData)
{
  Q_UNUSED(paletteNum);

  MainWindow* window = static_cast<MainWindow*>(userData);

  if (!window ||
      !pixmap)
  {
    return LS_OK;
  }

  // Track rendering state for subtitle counting
  static int regionCount = 0;
  static bool firstSubtitleDrawn = false;
  static QElapsedTimer startTime;

  if (!startTime.isValid())
  {
    startTime.start();
  }

  regionCount++;

  // Get elapsed time in milliseconds (for debugging)
  (void)startTime.elapsed();

  // Handle first subtitle (no cleanRegionCallback is called for it)
  if (!firstSubtitleDrawn)
  {
    window->m_subtitleCount = 1;      // Initialize counter for first subtitle
    firstSubtitleDrawn = true;
  }

  // Render immediately (synchronous)
  // Note: cleanRegionCallback was already called by the decoder before this
  window->m_subtitleWidget->renderPixmap(pixmap, palette);
  // Update display after each region
  window->m_subtitleWidget->update();
  window->updateStatusBar();
  return LS_OK;
}


// Global to track PCR state for file playback
static uint64_t g_current_pcr = 0;   // Current PCR value (always 0 for file playback)

LS_Status
MainWindow::getCurrentPCRCallback(uint64_t* currentPCR, void* userData)
{
  MainWindow* window = static_cast<MainWindow*>(userData);

  if (!window ||
      !currentPCR)
  {
    return LS_ERROR_GENERAL;
  }

  // For file playback with proper timing:
  // - PCR stays at 0 (no advancing PCR for file playback)
  // - PTS manager normalizes PTS values relative to first PTS
  // - First subtitle gets 2-second lead time added automatically
  *currentPCR = g_current_pcr;    // Always 0 for file playback
  return LS_OK;
}


LS_Status
MainWindow::ddsNotifyCallback(uint16_t displayWidth, uint16_t displayHeight, void* userData)
{
  MainWindow* window = static_cast<MainWindow*>(userData);

  if (!window)
  {
    return LS_ERROR_GENERAL;
  }

  // Store the DDS display dimensions
  window->m_ddsDisplayWidth = displayWidth;
  window->m_ddsDisplayHeight = displayHeight;
  window->m_ddsReceived = true;

  // Update SubtitleWidget with the correct display size for scaling
  if (window->m_subtitleWidget)
  {
    window->m_subtitleWidget->setDisplaySize(displayWidth, displayHeight);
  }

  return LS_OK;
}


void
MainWindow::closeEvent(QCloseEvent* event)
{
  // Stop playback first
  if (m_pesProcessingTimer)
  {
    m_pesProcessingTimer->stop();
  }

  // Process any pending events to let timers finish
  QCoreApplication::processEvents();
  // Cleanup decoder (will use flag to prevent double cleanup)
  cleanupDecoder();
  QMainWindow::closeEvent(event);
}


void
MainWindow::keyPressEvent(QKeyEvent* event)
{
  switch (event->key())
  {
    case Qt::Key_O:
      openFile();
      break;

    case Qt::Key_Q:
      close();
      break;

    default:
      QMainWindow::keyPressEvent(event);
  }
}
