#pragma once

#include <QPointer>
#include <QWidget>

#include <vector>

class QCheckBox;
class QComboBox;
class QDockWidget;
class QLabel;
class QPushButton;

class ReplayDock final : public QWidget {
public:
	explicit ReplayDock(QWidget *parent = nullptr);
	~ReplayDock() override;

	void handleFrontendEvent(int event);
	void refreshState();

private:
	struct DockVisibility {
		QPointer<QDockWidget> dock;
		bool wasVisible = false;
	};

	void buildUi();
	void loadUiSettings();
	void saveUiSettings() const;

	void toggleReplayBuffer();
	void saveReplay();
	void requestApplySettings();
	void applySettingsNow();
	void updateReplayLengthLive(int seconds);
	void setReplayMode(bool enabled);
	void updateStatusText(const QString &message = {});
	void updateLastReplay();
	void maybeAutoStart();

	QLabel *statusLabel_ = nullptr;
	QLabel *lastReplayLabel_ = nullptr;
	QPushButton *toggleBufferButton_ = nullptr;
	QPushButton *saveReplayButton_ = nullptr;
	QPushButton *applyButton_ = nullptr;
	QCheckBox *replayModeCheck_ = nullptr;
	QCheckBox *autoStartCheck_ = nullptr;
	QComboBox *resolutionCombo_ = nullptr;
	QComboBox *fpsCombo_ = nullptr;
	QComboBox *qualityCombo_ = nullptr;
	QComboBox *lengthCombo_ = nullptr;

	bool pendingApply_ = false;
	bool restartAfterApply_ = false;
	bool replayModeEnabled_ = false;
	std::vector<DockVisibility> hiddenDocks_;
};
