#include "replay-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>
#include <util/config-file.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>

namespace {
constexpr const char *kSettingsSection = "143Spotlight";

QString humanFileName(const char *path)
{
	if (!path || !*path)
		return QStringLiteral("No clips saved yet");

	QString value = QString::fromUtf8(path);
	const int slash = std::max(value.lastIndexOf('/'), value.lastIndexOf('\\'));
	return slash >= 0 ? value.mid(slash + 1) : value;
}

QString outputMode(config_t *config)
{
	const char *mode = config_get_string(config, "Output", "Mode");
	return QString::fromUtf8(mode ? mode : "Simple");
}

const char *replayConfigSection(config_t *config)
{
	return outputMode(config).compare(QStringLiteral("Advanced"), Qt::CaseInsensitive) == 0 ? "AdvOut"
	                                                                                         : "SimpleOutput";
}
}

ReplayDock::ReplayDock(QWidget *parent) : QWidget(parent)
{
	buildUi();
	loadUiSettings();
	refreshState();
}

ReplayDock::~ReplayDock()
{
	if (replayModeEnabled_)
		setReplayMode(false);
}

void ReplayDock::buildUi()
{
	setObjectName(QStringLiteral("143SpotlightWidget"));
	setMinimumWidth(280);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(14, 14, 14, 14);
	root->setSpacing(12);

	auto *title = new QLabel(QStringLiteral("143 Spotlight"), this);
	QFont titleFont = title->font();
	titleFont.setPointSize(titleFont.pointSize() + 4);
	titleFont.setBold(true);
	title->setFont(titleFont);
	root->addWidget(title);

	statusLabel_ = new QLabel(this);
	statusLabel_->setWordWrap(true);
	root->addWidget(statusLabel_);

	auto *buttonRow = new QHBoxLayout();
	toggleBufferButton_ = new QPushButton(this);
	saveReplayButton_ = new QPushButton(QStringLiteral("Save clip"), this);
	saveReplayButton_->setMinimumHeight(36);
	toggleBufferButton_->setMinimumHeight(36);
	buttonRow->addWidget(toggleBufferButton_, 1);
	buttonRow->addWidget(saveReplayButton_, 1);
	root->addLayout(buttonRow);

	connect(toggleBufferButton_, &QPushButton::clicked, this, [this]() { toggleReplayBuffer(); });
	connect(saveReplayButton_, &QPushButton::clicked, this, [this]() { saveReplay(); });

	auto *separator = new QFrame(this);
	separator->setFrameShape(QFrame::HLine);
	separator->setFrameShadow(QFrame::Sunken);
	root->addWidget(separator);

	auto *form = new QFormLayout();
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

	resolutionCombo_ = new QComboBox(this);
	resolutionCombo_->addItems({QStringLiteral("Native"), QStringLiteral("1440p"), QStringLiteral("1080p"),
	                            QStringLiteral("720p")});
	form->addRow(QStringLiteral("Resolution"), resolutionCombo_);

	fpsCombo_ = new QComboBox(this);
	fpsCombo_->addItems({QStringLiteral("30"), QStringLiteral("60"), QStringLiteral("120")});
	form->addRow(QStringLiteral("FPS"), fpsCombo_);

	qualityCombo_ = new QComboBox(this);
	qualityCombo_->addItems({QStringLiteral("Performance"), QStringLiteral("Balanced"), QStringLiteral("Quality")});
	form->addRow(QStringLiteral("Preset"), qualityCombo_);

	lengthCombo_ = new QComboBox(this);
	lengthCombo_->addItem(QStringLiteral("30 sec"), 30);
	lengthCombo_->addItem(QStringLiteral("60 sec"), 60);
	lengthCombo_->addItem(QStringLiteral("90 sec"), 90);
	lengthCombo_->addItem(QStringLiteral("120 sec"), 120);
	lengthCombo_->addItem(QStringLiteral("180 sec"), 180);
	form->addRow(QStringLiteral("Replay length"), lengthCombo_);
	root->addLayout(form);

	applyButton_ = new QPushButton(QStringLiteral("Apply"), this);
	root->addWidget(applyButton_);
	connect(applyButton_, &QPushButton::clicked, this, [this]() { requestApplySettings(); });

	connect(lengthCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
		updateReplayLengthLive(lengthCombo_->currentData().toInt());
		saveUiSettings();
	});
	connect(resolutionCombo_, &QComboBox::currentIndexChanged, this, [this](int) { saveUiSettings(); });
	connect(fpsCombo_, &QComboBox::currentIndexChanged, this, [this](int) { saveUiSettings(); });
	connect(qualityCombo_, &QComboBox::currentIndexChanged, this, [this](int) { saveUiSettings(); });

	replayModeCheck_ = new QCheckBox(QStringLiteral("Replay Mode (hide OBS docks)"), this);
	autoStartCheck_ = new QCheckBox(QStringLiteral("Start Replay Buffer with OBS"), this);
	root->addWidget(replayModeCheck_);
	root->addWidget(autoStartCheck_);

	connect(replayModeCheck_, &QCheckBox::toggled, this, [this](bool checked) {
		setReplayMode(checked);
		saveUiSettings();
	});
	connect(autoStartCheck_, &QCheckBox::toggled, this, [this](bool) { saveUiSettings(); });

	auto *lastTitle = new QLabel(QStringLiteral("Last clip"), this);
	QFont lastTitleFont = lastTitle->font();
	lastTitleFont.setBold(true);
	lastTitle->setFont(lastTitleFont);
	root->addWidget(lastTitle);

	lastReplayLabel_ = new QLabel(QStringLiteral("No clips saved yet"), this);
	lastReplayLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	lastReplayLabel_->setWordWrap(true);
	root->addWidget(lastReplayLabel_);
	root->addStretch(1);
}

void ReplayDock::loadUiSettings()
{
	config_t *config = obs_frontend_get_profile_config();
	if (!config)
		return;

	const char *resolution = config_get_string(config, kSettingsSection, "Resolution");
	const char *fps = config_get_string(config, kSettingsSection, "FPS");
	const char *quality = config_get_string(config, kSettingsSection, "Quality");
	const int replayLength = static_cast<int>(config_get_int(config, kSettingsSection, "ReplayLength"));
	const bool replayMode = config_get_bool(config, kSettingsSection, "ReplayMode");
	const bool autoStart = config_get_bool(config, kSettingsSection, "AutoStart");

	const QString resolutionValue = resolution && *resolution ? QString::fromUtf8(resolution) : QStringLiteral("1080p");
	const QString fpsValue = fps && *fps ? QString::fromUtf8(fps) : QStringLiteral("60");
	const QString qualityValue = quality && *quality ? QString::fromUtf8(quality) : QStringLiteral("Balanced");

	resolutionCombo_->setCurrentText(resolutionValue);
	fpsCombo_->setCurrentText(fpsValue);
	qualityCombo_->setCurrentText(qualityValue);

	const int wantedLength = replayLength > 0 ? replayLength : 90;
	for (int i = 0; i < lengthCombo_->count(); ++i) {
		if (lengthCombo_->itemData(i).toInt() == wantedLength) {
			lengthCombo_->setCurrentIndex(i);
			break;
		}
	}

	autoStartCheck_->setChecked(autoStart);
	replayModeCheck_->setChecked(replayMode);
}

void ReplayDock::saveUiSettings() const
{
	config_t *config = obs_frontend_get_profile_config();
	if (!config)
		return;

	config_set_string(config, kSettingsSection, "Resolution", resolutionCombo_->currentText().toUtf8().constData());
	config_set_string(config, kSettingsSection, "FPS", fpsCombo_->currentText().toUtf8().constData());
	config_set_string(config, kSettingsSection, "Quality", qualityCombo_->currentText().toUtf8().constData());
	config_set_int(config, kSettingsSection, "ReplayLength", lengthCombo_->currentData().toInt());
	config_set_bool(config, kSettingsSection, "ReplayMode", replayModeCheck_->isChecked());
	config_set_bool(config, kSettingsSection, "AutoStart", autoStartCheck_->isChecked());
	config_save_safe(config, "tmp", nullptr);
}

void ReplayDock::refreshState()
{
	const bool active = obs_frontend_replay_buffer_active();
	toggleBufferButton_->setText(active ? QStringLiteral("Stop buffer") : QStringLiteral("Start buffer"));
	saveReplayButton_->setEnabled(active);
	updateStatusText();
	updateLastReplay();
}

void ReplayDock::toggleReplayBuffer()
{
	if (obs_frontend_replay_buffer_active())
		obs_frontend_replay_buffer_stop();
	else
		obs_frontend_replay_buffer_start();

	updateStatusText(QStringLiteral("Changing Replay Buffer state…"));
}

void ReplayDock::saveReplay()
{
	if (!obs_frontend_replay_buffer_active()) {
		updateStatusText(QStringLiteral("Replay Buffer is not running."));
		return;
	}

	obs_frontend_replay_buffer_save();
	updateStatusText(QStringLiteral("Saving clip…"));
}

void ReplayDock::requestApplySettings()
{
	if (obs_frontend_streaming_active() || obs_frontend_recording_active()) {
		updateStatusText(QStringLiteral("Stop streaming/recording before changing video settings."));
		return;
	}

	saveUiSettings();
	pendingApply_ = true;
	restartAfterApply_ = obs_frontend_replay_buffer_active();

	if (restartAfterApply_) {
		updateStatusText(QStringLiteral("Applying settings and restarting Replay Buffer…"));
		obs_frontend_replay_buffer_stop();
		return;
	}

	applySettingsNow();
}

void ReplayDock::applySettingsNow()
{
	config_t *config = obs_frontend_get_profile_config();
	if (!config) {
		pendingApply_ = false;
		updateStatusText(QStringLiteral("Could not access the active OBS profile."));
		return;
	}

	const QString resolution = resolutionCombo_->currentText();
	uint64_t width = config_get_uint(config, "Video", "BaseCX");
	uint64_t height = config_get_uint(config, "Video", "BaseCY");

	if (resolution == QStringLiteral("1440p")) {
		width = 2560;
		height = 1440;
	} else if (resolution == QStringLiteral("1080p")) {
		width = 1920;
		height = 1080;
	} else if (resolution == QStringLiteral("720p")) {
		width = 1280;
		height = 720;
	}

	config_set_uint(config, "Video", "OutputCX", width);
	config_set_uint(config, "Video", "OutputCY", height);
	config_set_uint(config, "Video", "FPSType", 1);
	config_set_uint(config, "Video", "FPSInt", fpsCombo_->currentText().toUInt());

	const int replayLength = lengthCombo_->currentData().toInt();
	const char *rbSection = replayConfigSection(config);
	config_set_bool(config, rbSection, "RecRB", true);
	config_set_int(config, rbSection, "RecRBTime", replayLength);

	config_save_safe(config, "tmp", nullptr);
	obs_frontend_reset_video();
	updateReplayLengthLive(replayLength);

	pendingApply_ = false;
	updateStatusText(QStringLiteral("Settings applied. Quality preset is stored; encoder tuning comes next."));

	if (restartAfterApply_) {
		restartAfterApply_ = false;
		QTimer::singleShot(150, this, []() { obs_frontend_replay_buffer_start(); });
	}
}

void ReplayDock::updateReplayLengthLive(int seconds)
{
	config_t *config = obs_frontend_get_profile_config();
	if (config) {
		const char *section = replayConfigSection(config);
		config_set_int(config, section, "RecRBTime", seconds);
	}

	obs_output_t *output = obs_frontend_get_replay_buffer_output();
	if (!output)
		return;

	obs_data_t *settings = obs_output_get_settings(output);
	if (settings) {
		obs_data_set_int(settings, "max_time_sec", seconds);
		obs_output_update(output, settings);
		obs_data_release(settings);
	}
	obs_output_release(output);
}

void ReplayDock::setReplayMode(bool enabled)
{
	if (enabled == replayModeEnabled_)
		return;

	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow)
		return;

	if (enabled) {
		hiddenDocks_.clear();
		const auto docks = mainWindow->findChildren<QDockWidget *>();
		for (QDockWidget *dock : docks) {
			if (!dock || isAncestorOf(dock) || dock->isAncestorOf(this))
				continue;

			hiddenDocks_.push_back({QPointer<QDockWidget>(dock), dock->isVisible()});
			if (dock->isVisible())
				dock->hide();
		}
		replayModeEnabled_ = true;
		updateStatusText(QStringLiteral("Replay Mode enabled."));
	} else {
		for (const auto &entry : hiddenDocks_) {
			if (entry.dock && entry.wasVisible)
				entry.dock->show();
		}
		hiddenDocks_.clear();
		replayModeEnabled_ = false;
		updateStatusText(QStringLiteral("Standard OBS layout restored."));
	}
}

void ReplayDock::updateStatusText(const QString &message)
{
	if (!message.isEmpty()) {
		statusLabel_->setText(message);
		return;
	}

	statusLabel_->setText(obs_frontend_replay_buffer_active() ? QStringLiteral("● Replay Buffer active")
	                                                         : QStringLiteral("○ Replay Buffer stopped"));
}

void ReplayDock::updateLastReplay()
{
	char *path = obs_frontend_get_last_replay();
	lastReplayLabel_->setText(humanFileName(path));
	if (path)
		bfree(path);
}

void ReplayDock::maybeAutoStart()
{
	if (autoStartCheck_->isChecked() && !obs_frontend_replay_buffer_active()) {
		QTimer::singleShot(250, this, []() { obs_frontend_replay_buffer_start(); });
	}
}

void ReplayDock::handleFrontendEvent(int event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		maybeAutoStart();
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
		updateStatusText(QStringLiteral("Starting Replay Buffer…"));
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
		refreshState();
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
		updateStatusText(QStringLiteral("Stopping Replay Buffer…"));
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		refreshState();
		if (pendingApply_)
			QTimer::singleShot(0, this, [this]() { applySettingsNow(); });
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED:
		updateLastReplay();
		updateStatusText(QStringLiteral("Clip saved."));
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		loadUiSettings();
		refreshState();
		break;
	default:
		break;
	}
}
