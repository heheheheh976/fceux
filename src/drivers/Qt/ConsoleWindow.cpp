// GameApp.cpp
//
#include <QFileDialog>

#include "../../fceu.h"
#include "Qt/main.h"
#include "Qt/dface.h"
#include "Qt/input.h"
#include "Qt/ConsoleWindow.h"
#include "Qt/GamePadConf.h"
#include "Qt/HotKeyConf.h"
#include "Qt/ConsoleSoundConf.h"
#include "Qt/ConsoleVideoConf.h"
#include "Qt/fceuWrapper.h"
#include "Qt/keyscan.h"
#include "Qt/nes_shm.h"

consoleWin_t::consoleWin_t(QWidget *parent)
	: QMainWindow( parent )
{

	createMainMenu();

	viewport = new ConsoleViewGL_t(this);
	//viewport = new ConsoleViewSDL_t(this);

   setCentralWidget(viewport);

	gameTimer  = new QTimer( this );
	mutex      = new QMutex( QMutex::NonRecursive );
	emulatorThread = new emulatorThread_t();

   connect(emulatorThread, &QThread::finished, emulatorThread, &QObject::deleteLater);

	connect( gameTimer, &QTimer::timeout, this, &consoleWin_t::updatePeriodic );

	gameTimer->setTimerType( Qt::PreciseTimer );
	//gameTimer->start( 16 ); // 60hz
	gameTimer->start( 8 ); // 120hz

	emulatorThread->start();

   gamePadConfWin = NULL;
}

consoleWin_t::~consoleWin_t(void)
{
	nes_shm->runEmulator = 0;

   if ( gamePadConfWin != NULL )
   {
      gamePadConfWin->closeWindow();
   }
	fceuWrapperLock();
	fceuWrapperClose();
	fceuWrapperUnLock();

	//printf("Thread Finished: %i \n", gameThread->isFinished() );
	emulatorThread->quit();
	emulatorThread->wait();

	delete viewport;
	delete mutex;
}

void consoleWin_t::setCyclePeriodms( int ms )
{
	// If timer is already running, it will be restarted.
	gameTimer->start( ms );
   
	//printf("Period Set to: %i ms \n", ms );
}

void consoleWin_t::closeEvent(QCloseEvent *event)
{
   //printf("Main Window Close Event\n");
   if ( gamePadConfWin != NULL )
   {
      //printf("Command Game Pad Close\n");
      gamePadConfWin->closeWindow();
   }
   event->accept();

	closeApp();
}

void consoleWin_t::keyPressEvent(QKeyEvent *event)
{
   //printf("Key Press: 0x%x \n", event->key() );
	pushKeyEvent( event, 1 );
}

void consoleWin_t::keyReleaseEvent(QKeyEvent *event)
{
   //printf("Key Release: 0x%x \n", event->key() );
	pushKeyEvent( event, 0 );
}

//---------------------------------------------------------------------------
void consoleWin_t::createMainMenu(void)
{
    // This is needed for menu bar to show up on MacOS
	 menuBar()->setNativeMenuBar(false);

	 //-----------------------------------------------------------------------
	 // File
    fileMenu = menuBar()->addMenu(tr("File"));

	 // File -> Open ROM
	 openROM = new QAction(tr("Open ROM"), this);
    openROM->setShortcuts(QKeySequence::Open);
    openROM->setStatusTip(tr("Open ROM File"));
    connect(openROM, SIGNAL(triggered()), this, SLOT(openROMFile(void)) );

    fileMenu->addAction(openROM);

	 // File -> Close ROM
	 closeROM = new QAction(tr("Close ROM"), this);
    closeROM->setShortcut( QKeySequence(tr("Ctrl+C")));
    closeROM->setStatusTip(tr("Close Loaded ROM"));
    connect(closeROM, SIGNAL(triggered()), this, SLOT(closeROMCB(void)) );

    fileMenu->addAction(closeROM);

    fileMenu->addSeparator();

	 // File -> Quit
	 quitAct = new QAction(tr("Quit"), this);
    quitAct->setShortcut( QKeySequence(tr("Ctrl+Q")));
    quitAct->setStatusTip(tr("Quit the Application"));
    connect(quitAct, SIGNAL(triggered()), this, SLOT(closeApp()));

    fileMenu->addAction(quitAct);

	 //-----------------------------------------------------------------------
	 // Options
    optMenu = menuBar()->addMenu(tr("Options"));

	 // Options -> GamePad Config
	 gamePadConfig = new QAction(tr("GamePad Config"), this);
    //gamePadConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
    gamePadConfig->setStatusTip(tr("GamePad Configure"));
    connect(gamePadConfig, SIGNAL(triggered()), this, SLOT(openGamePadConfWin(void)) );

    optMenu->addAction(gamePadConfig);

	 // Options -> Sound Config
	 gameSoundConfig = new QAction(tr("Sound Config"), this);
    //gameSoundConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
    gameSoundConfig->setStatusTip(tr("Sound Configure"));
    connect(gameSoundConfig, SIGNAL(triggered()), this, SLOT(openGameSndConfWin(void)) );

    optMenu->addAction(gameSoundConfig);

	 // Options -> Video Config
	 gameVideoConfig = new QAction(tr("Video Config"), this);
    //gameVideoConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
    gameVideoConfig->setStatusTip(tr("Video Preferences"));
    connect(gameVideoConfig, SIGNAL(triggered()), this, SLOT(openGameVideoConfWin(void)) );

    optMenu->addAction(gameVideoConfig);

	 // Options -> HotKey Config
	 hotkeyConfig = new QAction(tr("Hotkey Config"), this);
    //hotkeyConfig->setShortcut( QKeySequence(tr("Ctrl+C")));
    hotkeyConfig->setStatusTip(tr("Hotkey Configure"));
    connect(hotkeyConfig, SIGNAL(triggered()), this, SLOT(openHotkeyConfWin(void)) );

    optMenu->addAction(hotkeyConfig);

	 // Options -> Auto-Resume
	 autoResume = new QAction(tr("Auto-Resume Play"), this);
    //autoResume->setShortcut( QKeySequence(tr("Ctrl+C")));
    autoResume->setCheckable(true);
    autoResume->setStatusTip(tr("Auto-Resume Play"));
    connect(autoResume, SIGNAL(triggered()), this, SLOT(toggleAutoResume(void)) );

    optMenu->addAction(autoResume);

    optMenu->addSeparator();

	 // Options -> Full Screen
	 fullscreen = new QAction(tr("Fullscreen"), this);
    fullscreen->setShortcut( QKeySequence(tr("Alt+Return")));
    //fullscreen->setCheckable(true);
    fullscreen->setStatusTip(tr("Fullscreen"));
    connect(fullscreen, SIGNAL(triggered()), this, SLOT(toggleFullscreen(void)) );

    optMenu->addAction(fullscreen);

	 //-----------------------------------------------------------------------
	 // Help
    helpMenu = menuBar()->addMenu(tr("Help"));

	 aboutAct = new QAction(tr("About"), this);
    aboutAct->setStatusTip(tr("About Qplot"));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(aboutQPlot(void)) );

    helpMenu->addAction(aboutAct);
};
//---------------------------------------------------------------------------
void consoleWin_t::closeApp(void)
{
	nes_shm->runEmulator = 0;

	fceuWrapperLock();
	fceuWrapperClose();
	fceuWrapperUnLock();

	// LoadGame() checks for an IP and if it finds one begins a network session
	// clear the NetworkIP field so this doesn't happen unintentionally
	g_config->setOption ("SDL.NetworkIP", "");
	g_config->save ();
	//SDL_Quit (); // Already called by fceuWrapperClose

	//qApp::quit();
	qApp->quit();
}
//---------------------------------------------------------------------------

void consoleWin_t::openROMFile(void)
{
	int ret;
	QString filename;
	QFileDialog  dialog(this, tr("Open ROM File") );

	dialog.setFileMode(QFileDialog::ExistingFile);

	dialog.setNameFilter(tr("All files (*.*) ;; NES files (*.nes)"));

	dialog.setViewMode(QFileDialog::List);

	// the gnome default file dialog is not playing nice with QT.
	// TODO make this a config option to use native file dialog.
	dialog.setOption(QFileDialog::DontUseNativeDialog, true);

	dialog.show();
	ret = dialog.exec();

	if ( ret )
	{
		QStringList fileList;
		fileList = dialog.selectedFiles();

		if ( fileList.size() > 0 )
		{
			filename = fileList[0];
		}
	}

   //filename =  QFileDialog::getOpenFileName( this,
   //       "Open ROM File",
   //       QDir::currentPath(),
   //       "All files (*.*) ;; NES files (*.nes)");
 
   if ( filename.isNull() )
   {
      return;
   }
	qDebug() << "selected file path : " << filename.toUtf8();

	g_config->setOption ("SDL.LastOpenFile", filename.toStdString().c_str() );

	fceuWrapperLock();
	CloseGame ();
	LoadGame ( filename.toStdString().c_str() );
	fceuWrapperUnLock();

   return;
}

void consoleWin_t::closeROMCB(void)
{
	fceuWrapperLock();
	CloseGame();
	fceuWrapperUnLock();
}

void consoleWin_t::openGamePadConfWin(void)
{
   if ( gamePadConfWin != NULL )
   {
      printf("GamePad Config Window Already Open\n");
      return;
   }
	//printf("Open GamePad Config Window\n");
   gamePadConfWin = new GamePadConfDialog_t(this);
	
   gamePadConfWin->show();
   gamePadConfWin->exec();

   delete gamePadConfWin;
   gamePadConfWin = NULL;
   //printf("GamePad Config Window Destroyed\n");
}

void consoleWin_t::openGameSndConfWin(void)
{
	ConsoleSndConfDialog_t *sndConfWin;

	//printf("Open Sound Config Window\n");
	
   sndConfWin = new ConsoleSndConfDialog_t(this);
	
   sndConfWin->show();
   sndConfWin->exec();

   delete sndConfWin;

   //printf("Sound Config Window Destroyed\n");
}

void consoleWin_t::openGameVideoConfWin(void)
{
	ConsoleVideoConfDialog_t *vidConfWin;

	//printf("Open Video Config Window\n");
	
   vidConfWin = new ConsoleVideoConfDialog_t(this);
	
   vidConfWin->show();
   vidConfWin->exec();

   delete vidConfWin;

   //printf("Video Config Window Destroyed\n");
}

void consoleWin_t::openHotkeyConfWin(void)
{
	HotKeyConfDialog_t *hkConfWin;

	//printf("Open Hot Key Config Window\n");
	
   hkConfWin = new HotKeyConfDialog_t(this);
	
   hkConfWin->show();
   hkConfWin->exec();

   delete hkConfWin;

   //printf("Hotkey Config Window Destroyed\n");
}

void consoleWin_t::toggleAutoResume(void)
{
   //printf("Auto Resume: %i\n", autoResume->isChecked() );

	g_config->setOption ("SDL.AutoResume", (int) autoResume->isChecked() );

	AutoResumePlay = autoResume->isChecked();
}

void consoleWin_t::toggleFullscreen(void)
{
	if ( isFullScreen() )
	{
		showNormal();
	}
	else
	{
		showFullScreen();
	}
}

void consoleWin_t::aboutQPlot(void)
{
   printf("About QPlot\n");
   return;
}

void consoleWin_t::updatePeriodic(void)
{
	//struct timespec ts;
	//double t;

	//clock_gettime( CLOCK_REALTIME, &ts );

	//t = (double)ts.tv_sec + (double)(ts.tv_nsec * 1.0e-9);
   //printf("Run Frame %f\n", t);
	
	// Update Input Devices
	FCEUD_UpdateInput();
	
	// RePaint Game Viewport
	if ( nes_shm->blitUpdated )
	{
		nes_shm->blitUpdated = 0;

		viewport->transfer2LocalBuffer();

		//viewport->repaint();
		viewport->update();
	}

   return;
}

void emulatorThread_t::run(void)
{
	printf("Emulator Start\n");
	nes_shm->runEmulator = 1;

	while ( nes_shm->runEmulator )
	{
		fceuWrapperUpdate();
	}
	printf("Emulator Exit\n");
	emit finished();
}
