/*-----------------------------------------------------------------------------
 * DVB Subtitle Player - Main Entry Point
 *
 * A Qt-based application to display DVB subtitles using libdvbsubdec.
 *---------------------------------------------------------------------------*/
/**
 * @file main.cpp
 * @brief DVB Subtitle Player - Qt Demo Application
 *
 * @section overview Overview
 *
 * This demo application demonstrates how to integrate the DVB subtitle decoder
 * library into a Qt-based multimedia application. It serves as a reference
 * implementation for developers who want to add DVB subtitle support to their
 * Qt applications.
 *
 * @section key_features Key Features
 *
 * - Qt5/Qt6-based GUI with video display window
 * - QTimer-based decoder timer integration (thread-safe via Qt event loop)
 * - PES (Packetized Elementary Stream) file playback with loop support
 * - Incremental PES processing for real-time subtitle display
 * - Keyboard shortcuts for playback control (Space, N, Escape)
 * - Command-line options for display size, playback speed, and verbose mode
 *
 * @section architecture Architecture
 *
 * The application follows this flow:
 *
 * 1. Initialization (main function)
 *    - Parse command-line arguments (file, size, speed, options)
 *    - Create QApplication instance
 *    - Create MainWindow with PES file path
 *    - Configure display settings and show window
 *    - Start Qt event loop (app.exec())
 *
 * 2. Playback (MainWindow::processNextPESPacket)
 *    - QTimer triggers at 40ms intervals (25fps)
 *    - Process one PES packet per trigger
 *    - Decoder callbacks update SubtitleWidget
 *    - SubtitleWidget renders regions with proper scaling
 *
 * 3. Cleanup (MainWindow::~MainWindow)
 *    - Stop all timers
 *    - Cleanup decoder service
 *    - Free PES buffer
 *
 * @section thread_safety Thread Safety
 *
 * Qt's QTimer mechanism ensures all decoder callbacks execute in the main
 * thread via the Qt event loop. This is thread-safe because:
 *
 * - QTimer callbacks run in the thread that created the timer (main thread)
 * - All decoder operations occur in the same thread
 * - No mutex protection needed for Qt signal/slot connections
 *
 * This is the CORRECT way to integrate with the decoder library:
 *
 * @code
 * // CORRECT: QTimer (safe - runs in main thread)
 * QTimer::singleShot(delay, this, [this]() {
 *     LS_DVBSubDecServicePlay(...);  // Safe!
 * });
 *
 * // WRONG: QThread with raw function (unsafe - different thread)
 * QThread::create([]() {
 *     LS_DVBSubDecServicePlay(...);  // CRASH!
 * });
 * @endcode
 *
 * @section integration_steps Integration Steps
 *
 * To integrate DVB subtitle decoding into your Qt application:
 *
 * @subsection step1 Step 1: Initialize Decoder Library
 *
 * @code
 * // Include the public API header
 * #include "lssubdec.h"
 *
 * // Implement system functions (Qt-based)
 * static int32_t qt_mutex_create(LS_Mutex_t* mutex) {
 *     *mutex = new QMutex();
 *     return LS_OK;
 * }
 *
 * static int32_t qt_timer_create(LS_Timer_t* timer_id,
 *                                void (*callback)(void*), void* param) {
 *     // Use QTimer-based timer implementation
 *     return qt_timer_create_qtimer(timer_id, callback, param);
 * }
 *
 * // Initialize library
 * LS_SystemFuncs_t sysFuncs = {
 *     .mutexCreateFunc = qt_mutex_create,
 *     .mutexDeleteFunc = qt_mutex_delete,
 *     .mutexWaitFunc = qt_mutex_wait,
 *     .mutexSignalFunc = qt_mutex_signal,
 *     .timerCreateFunc = qt_timer_create,
 *     .timerDeleteFunc = qt_timer_delete,
 *     .timerStartFunc = qt_timer_start,
 *     .timerStopFunc = qt_timer_stop,
 *     .getTimeStampFunc = qt_get_timestamp
 * };
 *
 * LS_DVBSubDecInit(bufferSize, sysFuncs);
 * @endcode
 *
 * @subsection step2 Step 2: Implement OSD Callbacks
 *
 * @code
 * static LS_Status clean_region_callback(LS_Rect_t rect, void* user_data) {
 *     MainWindow* window = (MainWindow*)user_data;
 *     window->subtitleWidget()->clearRegion(rect.leftPos, rect.topPos,
 *                                          rect.rightPos, rect.bottomPos);
 *     return LS_OK;
 * }
 *
 * static LS_Status draw_pixmap_callback(const LS_Pixmap_t* pixmap,
 *                                      const uint8_t* palette,
 *                                      uint8_t palette_num, void* user_data) {
 *     MainWindow* window = (MainWindow*)user_data;
 *     window->subtitleWidget()->addPixmap(pixmap, palette);
 *     return LS_OK;
 * }
 *
 * static LS_Status dds_notify_callback(uint16_t width, uint16_t height,
 *                                      void* user_data) {
 *     MainWindow* window = (MainWindow*)user_data;
 *     window->setDDSDimensions(width, height);
 *     return LS_OK;
 * }
 *
 * static LS_Status get_pcr_callback(uint64_t* pcr, void* user_data) {
 *     MainWindow* window = (MainWindow*)user_data;
 *     *pcr = window->currentPCR();
 *     return LS_OK;
 * }
 * @endcode
 *
 * @subsection step3 Step 3: Create and Start Decoder Service
 *
 * @code
 * // Create service with memory configuration
 * LS_ServiceMemCfg_t memCfg = {
 *     .codedDataBufferSize = 512 * 1024,
 *     .pixelBufferSize = 16 * 1024 * 1024,
 *     .compositionBufferSize = 2 * 1024 * 1024
 * };
 *
 * LS_ServiceID_t service = LS_DVBSubDecServiceNew(memCfg);
 *
 * // Setup OSD callbacks
 * LS_OSDRender_t osdRender = {
 *     .cleanRegionFunc = clean_region_callback,
 *     .cleanRegionFuncData = this,
 *     .drawPixmapFunc = draw_pixmap_callback,
 *     .drawPixmapFuncData = this,
 *     .ddsNotifyFunc = dds_notify_callback,
 *     .ddsNotifyFuncData = this,
 *     .getCurrentPCRFunc = get_pcr_callback,
 *     .getCurrentPCRFuncData = this,
 *     .OSDPixmapFormat = LS_PIXFMT_ARGB32,
 *     .alphaValueFullTransparent = 0,
 *     .alphaValueFullOpaque = 255
 * };
 *
 * LS_DVBSubDecServiceStart(service, osdRender);
 * @endcode
 *
 * @subsection step4 Step 4: Feed PES Data
 *
 * @code
 * // Process PES file incrementally
 * void processNextPESPacket() {
 *     // Find next PES packet (00 00 01 BD/BE)
 *     size_t packetStart = findNextPESPacket(currentPos);
 *     if (packetStart >= bufferSize) return;
 *
 *     // Get packet size
 *     size_t packetSize = getPESPacketSize(packetStart);
 *
 *     // Feed to decoder
 *     LS_CodedData_t pesData = {
 *         .data = pesBuffer + packetStart,
 *         .dataSize = packetSize
 *     };
 *     LS_PageId_t pageId = {0, 0};  // Wildcard
 *     LS_DVBSubDecServicePlay(service, &pesData, pageId);
 * }
 *
 * // Use QTimer to process packets periodically
 * QTimer* timer = new QTimer(this);
 * connect(timer, &QTimer::timeout, this, &MainWindow::processNextPESPacket);
 * timer->start(40);  // 25 fps
 * @endcode
 *
 * @subsection step5 Step 5: Render Subtitles
 *
 * @code
 * // SubtitleWidget handles rendering
 * class SubtitleWidget : public QWidget {
 *     void paintEvent(QPaintEvent* event) override {
 *         QPainter painter(this);
 *         painter.fillRect(rect(), Qt::black);  // Background
 *
 *         // Render all subtitle regions
 *         QMutexLocker locker(&m_mutex);
 *         for (const Region& region : m_regions) {
 *             QRect displayRect = calculateDisplayRect(region.originalRect);
 *             painter.drawImage(displayRect, region.image);
 *         }
 *     }
 * };
 * @endcode
 *
 * @subsection step6 Step 6: Cleanup
 *
 * @code
 * // Stop timers first
 * m_playbackTimer->stop();
 * m_pesProcessingTimer->stop();
 *
 * // Stop and delete decoder service
 * LS_DVBSubDecServiceStop(m_serviceId);
 * LS_DVBSubDecServiceDelete(m_serviceId);
 *
 * // Finalize library
 * LS_DVBSubDecFinalize();
 * @endcode
 *
 * @section command_line Command-Line Options
 *
 * @code
 * Usage: dvbplayer_qt [options] <pes_file>
 *
 * Options:
 *   -h, --help          Show help and exit
 *   -v, --version       Show version and exit
 *   -w, --width <W>     Display width (default: 720)
 *   -H, --height <H>    Display height (default: 576)
 *   -l, --loop          Loop playback
 *   --speed <N>         Playback speed multiplier (default: 1.0)
 *   --verbose           Verbose mode (show decoder events)
 *
 * Arguments:
 *   pes_file            Path to PES subtitle file
 * @endcode
 *
 * @section keyboard_shortcuts Keyboard Shortcuts
 *
 * - Space: Play/Pause
 * - N: Next frame (step one PES packet)
 * - Escape: Exit
 *
 * @section building Building
 *
 * @code
 * mkdir build && cd build
 * cmake -DBUILD_APP=ON ..
 * make dvbplayer_qt
 * @endcode
 *
 * @section usage Usage
 *
 * @code
 * ./dvbplayer_qt ../samples/490000000_subtitle_pid_205.pes
 * ./dvbplayer_qt --width 1920 --height 1080 subtitle.pes
 * ./dvbplayer_qt --loop --speed 0.5 subtitle.pes
 * @endcode
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QFont>
#include "MainWindow.h"

int
main(int argc, char* argv[])
{
  QApplication app(argc, argv);

  app.setApplicationName("DVB Subtitle Player");
  app.setApplicationVersion("1.0");
  app.setOrganizationName("dvbsubdec");

  // Set default font for better subtitle display
  QFont font = app.font();

  font.setPointSize(10);
  app.setFont(font);

  // Parse command line arguments
  QCommandLineParser parser;

  parser.setApplicationDescription("DVB Subtitle Player - Display DVB subtitle files");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument("pes_file", "Path to PES subtitle file");

  QCommandLineOption verboseOption("verbose", "Verbose mode (show decoder events)");

  parser.addOption(verboseOption);

  QCommandLineOption widthOption(QStringList() << "w" << "width", "Display width", "width", "720");

  parser.addOption(widthOption);

  QCommandLineOption heightOption(QStringList() << "H" << "height", "Display height", "height", "576");

  parser.addOption(heightOption);

  QCommandLineOption loopOption(QStringList() << "l" << "loop", "Loop playback");

  parser.addOption(loopOption);

  QCommandLineOption speedOption("speed", "Playback speed multiplier", "speed", "1.0");

  parser.addOption(speedOption);
  parser.process(app);

  const QStringList args = parser.positionalArguments();

  if (args.isEmpty())
  {
    fprintf(stderr, "Error: No PES file specified\n\n");
    parser.showHelp(1);
    return 1;
  }

  // Get settings
  QString pesFile = args.first();
  int width = parser.value(widthOption).toInt();
  int height = parser.value(heightOption).toInt();
  bool verbose = parser.isSet(verboseOption);
  bool loop = parser.isSet(loopOption);
  double speed = parser.value(speedOption).toDouble();
  // Create and show main window
  MainWindow window(pesFile);

  window.setDisplaySize(width, height);
  window.setVerbose(verbose);
  window.setLoop(loop);
  window.setPlaybackSpeed(speed);
  window.show();
  return app.exec();
}
