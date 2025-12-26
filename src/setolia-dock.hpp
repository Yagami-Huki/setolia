#include "../deps/httplib.h"

#include <QWidget>

class QScrollArea;
class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QCheckBox;
class QTimer;
class QTextEdit;
class QPushButton;
class QComboBox;

class SETOLIA_Dock : public QWidget {
	Q_OBJECT
private:
	QVBoxLayout *m_Layout;
	QScrollArea *m_ScrollArea;
	QWidget *m_Content;
	QVBoxLayout *m_ContentLayout;

	QLabel *clock_Label;
	QLineEdit *clock_Display;
	QCheckBox *clock_Toggle;
	QTimer *clock_Timer;
	void startClock();
	void stopClock();

	QLabel *reserve_Label;
	QTextEdit *reserve_Text;

	QLabel *singing_Label;
	QLineEdit *singing_Text;

	QLabel *setlist_Label;
	QTextEdit *setlist_Text;

	QPushButton *next_Button;
	QCheckBox *next_Toggle;

	QLineEdit *timestamp_Display;
	QCheckBox *timestamp_Toggle;
	QTimer *timestamp_Timer;
	int timestamp_Count = 0;

	QPushButton *reset_Button;

	QComboBox *template_ComboBox;
	QPushButton *open_Template_Button;
	QPushButton *reload_Template_Button;
	void loadTemplates();

	QPushButton *add_Button;

	QLabel *lyricSearch_Label;
	QHBoxLayout *lyricSearch_Layout;
	QLineEdit *lyricSearch_Text;
	QPushButton *lyricSearch_Button;

	httplib::Server srv;
	std::thread srv_thread;
	std::mutex srv_mutex;
	std::condition_variable srv_cond;
	std::string latestData;
	std::string cachedData;
	std::mutex data_mutex;
	bool srv_stopped = false;
	QStringList splitTimastamp(QString text);
	void updateCachedData();
	void startServer();
	void stopServer();
	void sendData();

public:
	SETOLIA_Dock(QWidget *parent);
	~SETOLIA_Dock();

	void nextSong();

	void StartTimer();
	void StopTimer();
};
