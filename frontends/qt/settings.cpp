/*
 * Copyright 2024 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * Implementation of netsurf settings window for qt.
 */

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QScrollBar>
#include <QFormLayout>
#include <QAbstractButton>
#include <QToolButton>
#include <QFile>
#include <QCheckBox>
#include <QComboBox>

extern "C" {
#include "utils/nsoption.h"
#include "utils/messages.h"

#include "desktop/searchweb.h"
}

#include "qt/application.cls.h"
#include "qt/settings.cls.h"
#include "qt/listselection.cls.h"

/**
 * construct an accept_language string from a selection
 *
 * \param selected The list of selected languages
 * \return accept_language string or NULL on error or no selection
 */
static char *accept_language_from_selection(const QList<QByteArray> &selected)
{
	int idx;
	int totallen = 0;
	char *alang;
	char *cur;
	float quality = 1.0;

	/* compute output buffer length */
	for (idx = 0; idx < selected.size(); idx++) {
		totallen += selected.at(idx).size();
		totallen += 8; /* allow for ';q=0.0, ' */
	}
	if (totallen == 0) {
		return NULL;
	}

	alang = (char *)malloc(totallen);
	if (alang != NULL) {
		cur = alang;
		for (idx = 0; idx < selected.size(); idx++) {
			int outlen;
			quality -= 0.1;
			if (quality <= 0.2) {
				quality = 0.2;
			}
			outlen = snprintf(cur,
					  totallen,
					  "%.*s;q=%.1f, ",
					  (int)selected.at(idx).size(),
					  selected.at(idx).constData(),
					  quality);
			cur+=outlen;
			totallen-=outlen;
		}
		cur-=2;
		*cur=0;
	}
	return alang;
}


class GeneralSettings: public AbstractSettingsCategory
{
public:
	GeneralSettings(QWidget *parent=nullptr);
	const char *categoryName() { return messages_get("General"); }
	void categoryRealize();
	void categoryApply();
private:
	QCheckBox *m_enablejavascript;
};


GeneralSettings::GeneralSettings(QWidget *parent)
	: AbstractSettingsCategory(parent),
	  m_enablejavascript(new QCheckBox)
{
	m_enablejavascript->setText(messages_get("Enable Javascript"));

	QFormLayout *browsinglayout = new QFormLayout;
	browsinglayout->addWidget(m_enablejavascript);

	QGroupBox *browsinggroup = new QGroupBox(messages_get("Browsing"));
	browsinggroup->setFlat(true);
	browsinggroup->setLayout(browsinglayout);

	QFormLayout *layout = new QFormLayout;

	QGroupBox *dlgroup = new QGroupBox(messages_get("Downloads"));
	dlgroup->setFlat(true);
	dlgroup->setLayout(layout);

	QVBoxLayout *category_v = new QVBoxLayout;
	category_v->addWidget(browsinggroup);
	category_v->addWidget(dlgroup);

	setLayout(category_v);
}


void GeneralSettings::categoryRealize()
{
	enum Qt::CheckState state = Qt::Unchecked;
	if (nsoption_bool(enable_javascript)) {
		state = Qt::Checked;
	}
	m_enablejavascript->setCheckState(state);

}


void GeneralSettings::categoryApply()
{
	bool checked = false;
	if (m_enablejavascript->checkState() == Qt::Checked) {
		checked = true;
	}
	nsoption_set_bool(enable_javascript, checked);
}


class HomeSettings: public AbstractSettingsCategory
{
public:
	HomeSettings(QWidget *parent=nullptr);

	const char *categoryName() { return messages_get("Home"); }
	void categoryRealize();
	void categoryApply();
private:
	QLineEdit *m_homeurl;
};

void HomeSettings::categoryRealize()
{
	m_homeurl->setText(nsoption_charp(homepage_url));
}

void HomeSettings::categoryApply()
{
	if (m_homeurl->isModified()) {
		nsoption_set_charp(homepage_url,
				   strdup(m_homeurl->text().toStdString().c_str()));
	}
}

HomeSettings::HomeSettings(QWidget *parent)
	: AbstractSettingsCategory(parent),
	  m_homeurl(new QLineEdit)
{
	QFormLayout *pagelayout = new QFormLayout;
	pagelayout->addRow("Homepage", m_homeurl);

	QGroupBox *pagegroup = new QGroupBox("New windows and tabs");
	pagegroup->setFlat(true);
	pagegroup->setLayout(pagelayout);

	QVBoxLayout *category_v = new QVBoxLayout;
	category_v->addWidget(pagegroup);

	setLayout(category_v);
}


class AppearanceSettings: public AbstractSettingsCategory
{
public:
	AppearanceSettings(QWidget *parent=nullptr);
	const char *categoryName() { return "Appearance"; }
	void categoryRealize();
	void categoryApply();
private:
	QCheckBox *m_opentab;
	QCheckBox *m_switchnew;
	QComboBox *m_colour_selection;
	QComboBox *m_zoom;
};

AppearanceSettings::AppearanceSettings(QWidget *parent)
	: AbstractSettingsCategory(parent),
	  m_opentab(new QCheckBox),
	  m_switchnew(new QCheckBox),
	  m_colour_selection(new QComboBox),
	  m_zoom(new QComboBox)
{
	m_opentab->setText(messages_get("TabLinkOpen"));

	m_switchnew->setText(messages_get("TabSwitchNew"));

	int scales[]={ 33, 50, 67, 75, 80, 90, 100, 110, 120, 133, 150, 170, 200, 240, 300, 400, 500, 0 };
	for (int idx=0; scales[idx] != 0; idx++) {
		m_zoom->addItem(QString::number(scales[idx])+"%", scales[idx]);
	}

	m_colour_selection->addItem(messages_get("ColourSelectionAutomatic"));
	m_colour_selection->addItem(messages_get("ColourSelectionLight"));
	m_colour_selection->addItem(messages_get("ColourSelectionDark"));

	QFormLayout *tabslayout = new QFormLayout;
	tabslayout->addWidget(m_opentab);
	tabslayout->addWidget(m_switchnew);

	QGroupBox *tabsgroup = new QGroupBox(messages_get("Tabs"));
	tabsgroup->setFlat(true);
	tabsgroup->setLayout(tabslayout);

	QFormLayout *colourlayout = new QFormLayout;
	colourlayout->addRow(messages_get("ColourSelection"), m_colour_selection);

	QGroupBox *colourgroup = new QGroupBox(messages_get("Colours"));
	colourgroup->setFlat(true);
	colourgroup->setLayout(colourlayout);

	QFormLayout *zoomlayout = new QFormLayout;
	zoomlayout->addRow(messages_get("DefaultScale"), m_zoom);

	QGroupBox *zoomgroup = new QGroupBox(messages_get("ScaleNS"));
	zoomgroup->setFlat(true);
	zoomgroup->setLayout(zoomlayout);

	QVBoxLayout *category_v = new QVBoxLayout;
	category_v->addWidget(tabsgroup);
	category_v->addWidget(colourgroup);
	category_v->addWidget(zoomgroup);

	setLayout(category_v);
}


void AppearanceSettings::categoryRealize()
{
	// tabs settings
	enum Qt::CheckState state = Qt::Unchecked;
	if (nsoption_bool(button_2_tab)) {
		state = Qt::Checked;
	}
	m_opentab->setCheckState(state);

	state = Qt::Unchecked;
	if (nsoption_bool(foreground_new)) {
		state = Qt::Checked;
	}
	m_switchnew->setCheckState(state);

	// colour selection
	m_colour_selection->setCurrentIndex(nsoption_uint(colour_selection));

	/* select the page scale value in combobox closest to user config */
	int sel_idx=0;
	int max_difference=INT_MAX;
	for (int idx = 0; idx < m_zoom->count() ; idx++) {
		int delta = abs(m_zoom->itemData(idx).toInt() - nsoption_int(scale));
		if (delta < max_difference) {
			max_difference = delta;
			sel_idx = idx;
		}
	}
	m_zoom->setCurrentIndex(sel_idx);
}


void AppearanceSettings::categoryApply()
{
	bool checked = false;
	if (m_opentab->checkState() == Qt::Checked) {
		checked = true;
	}
	nsoption_set_bool(button_2_tab, checked);

	checked = false;
	if (m_switchnew->checkState() == Qt::Checked) {
		checked = true;
	}
	nsoption_set_bool(foreground_new, checked);

	nsoption_set_uint(colour_selection, m_colour_selection->currentIndex());
	NS_Application::instance()->nsOptionUpdate();

	nsoption_set_int(scale, m_zoom->currentData().toInt());
}


class LanguageSettings: public AbstractSettingsCategory
{
public:
	LanguageSettings(QWidget *parent=nullptr);
	const char *categoryName() { return "Language"; }
	void categoryRealize();
	void categoryApply();
private:
	NS_ListSelection *m_pagelang;
};

LanguageSettings::LanguageSettings(QWidget *parent)
	: AbstractSettingsCategory(parent),
	  m_pagelang(new NS_ListSelection)
{
	QFile lang(":languages");
	if (!lang.open(QIODevice::ReadOnly | QIODevice::Text)) {
		m_pagelang->addItem("Deutsch", "de");
		m_pagelang->addItem("English", "en");
		m_pagelang->addItem("français", "fr");
		m_pagelang->addItem("italiano", "it");
		m_pagelang->addItem("Nederlands", "nl");
		m_pagelang->addItem("中文（简体，中国）", "zh-CN");
	} else {
		while (!lang.atEnd()) {
			QByteArray line = lang.readLine();
			if (line.size() <= 1) {
				continue;
			}
			if (line.front() == '#') {
				continue;
			}
			if (line.back() == '\n') {
				line.remove(line.size() - 1, 1);
			}
			QList<QByteArray> split = line.split(':');
			switch (split.size()) {
			default:
			case 1:
				m_pagelang->addItem(split.at(0), split.at(0));
				break;
			case 2:
				m_pagelang->addItem(split.at(1), split.at(0));
				break;
			case 3:
				m_pagelang->addItem(split.at(2), split.at(0));
				break;
			}
		}
		lang.close();
	}

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(m_pagelang);

	QGroupBox *group = new QGroupBox("Web page language");
	group->setFlat(true);
	group->setLayout(layout);

	QVBoxLayout *category_v = new QVBoxLayout;
	category_v->addWidget(group);

	setLayout(category_v);
}


void LanguageSettings::categoryRealize()
{
	m_pagelang->deselectAll();

	QByteArray alang(nsoption_charp(accept_language));
	if (alang.size() == 0) {
		return;
	}
	QList<QByteArray> split = alang.split(',');
	for (int idx=0; idx < split.size(); idx++) {
		QByteArray ent = split.at(idx);
		int langlen;
		// strip leading spaces
		while ((ent.size() > 0) && (ent.front() == ' ')) {
			ent.remove(0,1);
		}
		// remove trailing quality, etc
		for (langlen=0;langlen < ent.size();langlen++) {
			if ((ent[langlen] < 'a' || ent[langlen] > 'z') &&
			    (ent[langlen] < 'A' || ent[langlen] > 'Z') &&
			    (ent[langlen] < '0' || ent[langlen] > '9') &&
			    (ent[langlen] != '-')) {
				break;
			}
		}
		ent.remove(langlen, ent.size() - langlen);

		if (ent.size() > 0) {
			m_pagelang->selectItem(ent);
		}
	}
}


void LanguageSettings::categoryApply()
{
	char *alang;

	alang = accept_language_from_selection(m_pagelang->selection());

	if (alang != NULL) {
		nsoption_set_charp(accept_language, alang);
	}
}


class SearchSettings: public AbstractSettingsCategory
{
public:
	SearchSettings(QWidget *parent=nullptr);
	const char *categoryName() { return "Search"; }
	void categoryRealize();
	void categoryApply();
private:
	QComboBox *m_provider; /**< web search provider */
};

SearchSettings::SearchSettings(QWidget *parent)
	: AbstractSettingsCategory(parent),
	  m_provider(new QComboBox)
{
	int iter;
	const char *name;
	iter = search_web_iterate_providers(-1, &name);
	while (iter != -1) {
		m_provider->addItem(name);
		iter = search_web_iterate_providers(iter, &name);
	}

	QFormLayout *layout = new QFormLayout;
	layout->addRow("Web Search Provider", m_provider);

	QGroupBox *searchgroup = new QGroupBox("Web Search");
	searchgroup->setFlat(true);
	searchgroup->setLayout(layout);

	QVBoxLayout *category_v = new QVBoxLayout;
	category_v->addWidget(searchgroup);

	setLayout(category_v);
}


void SearchSettings::categoryRealize()
{
	for (int idx = 0; idx < m_provider->count() ; idx++) {
		if (m_provider->itemText(idx) == nsoption_charp(search_web_provider)) {
			m_provider->setCurrentIndex(idx);
			break;
		}
	}
}


void SearchSettings::categoryApply()
{
	char *provider = strdup(m_provider->currentText().toUtf8().data());
	search_web_select_provider(provider);
	nsoption_set_charp(search_web_provider, provider);
}


class PrivacySettings: public AbstractSettingsCategory
{
public:
	PrivacySettings(QWidget *parent=nullptr);
	const char *categoryName() { return messages_get("Privacy"); }
	void categoryRealize();
	void categoryApply();
private:
	QCheckBox *m_preventpopups;
	QCheckBox *m_hideadverts;
	QCheckBox *m_enablednt;
	QCheckBox *m_enablereferral;
};

PrivacySettings::PrivacySettings(QWidget *parent)
	: AbstractSettingsCategory(parent),
	  m_preventpopups(new QCheckBox),
	  m_hideadverts(new QCheckBox),
	  m_enablednt(new QCheckBox),
	  m_enablereferral(new QCheckBox)
{
	m_preventpopups->setText(messages_get("Prevent popups"));
	m_hideadverts->setText(messages_get("Hide adverts"));
	m_enablednt->setText(messages_get("Enable Do Not Track"));
	m_enablereferral->setText(messages_get("Enable sending referrer"));

	QFormLayout *genlayout = new QFormLayout;
	genlayout->addWidget(m_preventpopups);
	genlayout->addWidget(m_hideadverts);

	QGroupBox *gengroup = new QGroupBox(messages_get("General"));
	gengroup->setFlat(true);
	gengroup->setLayout(genlayout);

	QFormLayout *sitelayout = new QFormLayout;
	sitelayout->addWidget(m_enablednt);
	sitelayout->addWidget(m_enablereferral);

	QGroupBox *sitegroup = new QGroupBox(messages_get("Site"));
	sitegroup->setFlat(true);
	sitegroup->setLayout(sitelayout);

	QVBoxLayout *category_v = new QVBoxLayout;
	category_v->addWidget(gengroup);
	category_v->addWidget(sitegroup);

	setLayout(category_v);
}


void PrivacySettings::categoryRealize()
{
	enum Qt::CheckState state;

	if (nsoption_bool(disable_popups)) {
		state = Qt::Checked;
	} else {
		state = Qt::Unchecked;
	}
	m_preventpopups->setCheckState(state);

	if (nsoption_bool(block_advertisements)) {
		state = Qt::Checked;
	} else {
		state = Qt::Unchecked;
	}
	m_hideadverts->setCheckState(state);

	if (nsoption_bool(do_not_track)) {
		state = Qt::Checked;
	} else {
		state = Qt::Unchecked;
	}
	m_enablednt->setCheckState(state);

	if (nsoption_bool(send_referer)) {
		state = Qt::Checked;
	} else {
		state = Qt::Unchecked;
	}
	m_enablereferral->setCheckState(state);
}


void PrivacySettings::categoryApply()
{
	nsoption_set_bool(disable_popups,
			  (m_preventpopups->checkState() == Qt::Checked));

	nsoption_set_bool(block_advertisements,
			  (m_hideadverts->checkState() == Qt::Checked));

	nsoption_set_bool(do_not_track,
			  (m_enablednt->checkState() == Qt::Checked));

	nsoption_set_bool(send_referer,
			  (m_enablereferral->checkState() == Qt::Checked));
}


class NetworkSettings:public AbstractSettingsCategory
{
public:
	NetworkSettings(QWidget *parent=nullptr);

	const char *categoryName() { return "Network"; }
	void categoryRealize();
	void categoryApply();
private slots:
	void proxy_access_changed(int index);

private:
	QFormLayout *m_proxylayout; /**< layout containing proxy */
	QWidget *m_hostport; /**< widget holding the host and port layout */
	QComboBox *m_proxy_access; /**< proxy access type direct/manual */
	QLineEdit *m_proxy_host;
	QSpinBox *m_proxy_port;
	QLineEdit *m_proxy_auth_user;
	QLineEdit *m_proxy_auth_pass;
	QLineEdit *m_proxy_noproxy;

	QSpinBox *m_fetchers_max;
	QSpinBox *m_fetchers_perhost;
	QSpinBox *m_fetchers_cached;

};

void NetworkSettings::proxy_access_changed(int index)
{
	switch(index) {
	case 0: //Direct
		m_proxylayout->setRowVisible(m_hostport, false);
		m_proxylayout->setRowVisible(m_proxy_auth_user, false);
		m_proxylayout->setRowVisible(m_proxy_auth_pass, false);
		m_proxylayout->setRowVisible(m_proxy_noproxy, false);
		break;
	case 1: //Manual no auth
		m_proxylayout->setRowVisible(m_hostport, true);
		m_proxylayout->setRowVisible(m_proxy_auth_user, false);
		m_proxylayout->setRowVisible(m_proxy_auth_pass, false);
		m_proxylayout->setRowVisible(m_proxy_noproxy, true);
		break;
	case 2: //Manual with basic auth
	case 3: //Manual with ntlm auth
		m_proxylayout->setRowVisible(m_hostport, true);
		m_proxylayout->setRowVisible(m_proxy_auth_user, true);
		m_proxylayout->setRowVisible(m_proxy_auth_pass, true);
		m_proxylayout->setRowVisible(m_proxy_noproxy, true);
		break;
	}
}

void NetworkSettings::categoryRealize()
{
	//proxy
	m_proxy_host->setText(nsoption_charp(http_proxy_host));
	m_proxy_port->setValue(nsoption_int(http_proxy_port));
	m_proxy_auth_user->setText(nsoption_charp(http_proxy_auth_user));
	m_proxy_auth_pass->setText(nsoption_charp(http_proxy_auth_pass));
	m_proxy_noproxy->setText(nsoption_charp(http_proxy_noproxy));

	if (nsoption_bool(http_proxy)) {
		// Manual configuration
		switch (nsoption_int(http_proxy_auth)) {
		case OPTION_HTTP_PROXY_AUTH_NONE:
		default:
			m_proxy_access->setCurrentIndex(1);
			proxy_access_changed(1);
			break;

		case OPTION_HTTP_PROXY_AUTH_BASIC:
			m_proxy_access->setCurrentIndex(2);
			proxy_access_changed(2);
			break;

		case OPTION_HTTP_PROXY_AUTH_NTLM:
			m_proxy_access->setCurrentIndex(3);
			proxy_access_changed(3);
			break;
		}
	} else {
		m_proxy_access->setCurrentIndex(0); //Direct
		proxy_access_changed(0);
	}

	//fetchers
	m_fetchers_max->setValue(nsoption_int(max_fetchers));
	m_fetchers_perhost->setValue(nsoption_int(max_fetchers_per_host));
	m_fetchers_cached->setValue(nsoption_int(max_cached_fetch_handles));
}

void NetworkSettings::categoryApply()
{
	//proxy
	int access = m_proxy_access->currentIndex();
	if (access == 0) {
		nsoption_set_bool(http_proxy, false);
	} else {
		nsoption_set_bool(http_proxy, true);
		if (m_proxy_host->isModified()) {
			nsoption_set_charp(http_proxy_host,
					   strdup(m_proxy_host->text().toStdString().c_str()));
		}
		nsoption_set_int(http_proxy_port, m_proxy_port->value());
		if (m_proxy_auth_user->isModified()) {
			nsoption_set_charp(http_proxy_auth_user,
					   strdup(m_proxy_auth_user->text().toStdString().c_str()));
		}
		if (access == 1) {
			nsoption_set_int(http_proxy_auth,
					 OPTION_HTTP_PROXY_AUTH_NONE);
		} else {
			if (access == 3) {
				nsoption_set_int(http_proxy_auth,
						 OPTION_HTTP_PROXY_AUTH_NTLM);
			} else {
				nsoption_set_int(http_proxy_auth,
						 OPTION_HTTP_PROXY_AUTH_BASIC);
			}
			if (m_proxy_auth_pass->isModified()) {
				nsoption_set_charp(http_proxy_auth_pass,
						   strdup(m_proxy_auth_pass->text().toStdString().c_str()));
			}
			if (m_proxy_noproxy->isModified()) {
				nsoption_set_charp(http_proxy_noproxy,
						   strdup(m_proxy_noproxy->text().toStdString().c_str()));
			}
		}
	}

	// fetchers
	nsoption_set_int(max_fetchers, m_fetchers_max->value());
	nsoption_set_int(max_fetchers_per_host, m_fetchers_perhost->value());
	nsoption_set_int(max_cached_fetch_handles, m_fetchers_cached->value());
}

NetworkSettings::NetworkSettings(QWidget *parent)
	: AbstractSettingsCategory(parent),
	  m_proxylayout(new QFormLayout),
	  m_hostport(new QWidget),
	  m_proxy_access(new QComboBox),
	  m_proxy_host(new QLineEdit),
	  m_proxy_port(new QSpinBox),
	  m_proxy_auth_user(new QLineEdit),
	  m_proxy_auth_pass(new QLineEdit),
	  m_proxy_noproxy(new QLineEdit)
{
	//Proxy

	connect(m_proxy_access, &QComboBox::currentIndexChanged,
		this, &NetworkSettings::proxy_access_changed);
	m_proxy_port->setRange(1, 65535);

	m_proxy_access->addItem("Direct Connection");
	m_proxy_access->addItem("Manual Configuration");
	m_proxy_access->addItem("Manual Configuration with basic authentication");
	m_proxy_access->addItem("Manual Configuration with NTLM authentication");
	m_proxylayout->addRow("Proxy access to internet", m_proxy_access);
	// Access: direct/system/manual no auth/manual basic auth/manual ntlm auth
	// Proxy Type:http/http1/https/https2/socks4/socks4a/socks5/socks5h
	// Host: host:port

	QHBoxLayout *hostportlayout = new QHBoxLayout(m_hostport);
	hostportlayout->setContentsMargins(0,0,0,0);
	QLabel *portlabel = new QLabel(":");
	hostportlayout->addWidget(m_proxy_host);
	hostportlayout->addWidget(portlabel);
	hostportlayout->addWidget(m_proxy_port);

	m_proxylayout->addRow("Host", m_hostport);
        // username
	m_proxylayout->addRow("Username", m_proxy_auth_user);
	// password
	m_proxylayout->addRow("Password", m_proxy_auth_pass);
	// no proxy:
	m_proxylayout->addRow("No proxy for", m_proxy_noproxy);

	QGroupBox *proxygroup = new QGroupBox(messages_get("Proxy"));
	proxygroup->setFlat(true);
	proxygroup->setLayout(m_proxylayout);

	//fetchers
	m_fetchers_max = new QSpinBox;
	m_fetchers_perhost = new QSpinBox;
	m_fetchers_cached = new QSpinBox;

	QFormLayout *fetcherslayout = new QFormLayout;
	fetcherslayout->addRow("Maximum",m_fetchers_max);
	//maximum fetchers
	fetcherslayout->addRow("Per host",m_fetchers_perhost);
	//fetchers per host
	fetcherslayout->addRow("Cached",m_fetchers_cached);
	//cached connections

	QGroupBox *fetchersgroup = new QGroupBox("Fetchers");
	fetchersgroup->setFlat(true);
	fetchersgroup->setLayout(fetcherslayout);

	QVBoxLayout *network_v = new QVBoxLayout;
	network_v->addWidget(proxygroup);
	network_v->addWidget(fetchersgroup);

	setLayout(network_v);
}


/**
 * Netsurf browser settings window.
 *
 * Provides a native user interface to alter browser settings.
 *
 * The settings are divided into catagories and each category is placed in a
 * list where selecting the category changes the page shown in a widget stack.
 */
NS_Settings::NS_Settings(QWidget *parent)
	: QWidget(parent),
	  m_headerlabel(new QLabel),
	  m_categorylist(new FirstListWidget),
	  m_categories(new QStackedWidget),
	  m_buttonbox(new QDialogButtonBox(QDialogButtonBox::Ok |
					   QDialogButtonBox::Cancel |
					   QDialogButtonBox::Apply))
{
	connect(m_categorylist, &QListWidget::currentRowChanged,
		m_categories, &QStackedWidget::setCurrentIndex);

	connect(m_categorylist, &QListWidget::currentTextChanged,
		this, &NS_Settings::categorychanged_slot);

	connect(m_buttonbox, &QDialogButtonBox::clicked,
		this, &NS_Settings::clicked_slot);

	QFont labelfont;
	labelfont.setBold(true);
	m_headerlabel->setFont(labelfont);

	QGridLayout *layout = new QGridLayout;
	layout->addWidget( m_categorylist, 0, 0, 2, 1);
	layout->addWidget(  m_headerlabel, 0, 1, 1, 1);
	layout->addWidget(   m_categories, 1, 1, 1, 1);
	layout->addWidget(    m_buttonbox, 2, 0, 1, 2);
	layout->setColumnStretch(1, 4);
	setLayout(layout);

	/* add categories */
	addCategory(new GeneralSettings);
	addCategory(new HomeSettings);
	addCategory(new AppearanceSettings);
	addCategory(new LanguageSettings);
	addCategory(new SearchSettings);
	addCategory(new PrivacySettings);
	addCategory(new NetworkSettings);
}

/**
 * reset form values to current nsoption values
 */
void NS_Settings::showEvent(QShowEvent *event)
{
	for (int idx = 0; idx < m_categories->count(); idx++) {
		dynamic_cast<AbstractSettingsCategory *>
			(m_categories->widget(idx))->categoryRealize();
	}
}


void NS_Settings::clicked_slot(QAbstractButton *button)
{
	QDialogButtonBox::ButtonRole role = m_buttonbox->buttonRole(button);
	if ((role == QDialogButtonBox::AcceptRole) ||
	    (role == QDialogButtonBox::ApplyRole)) {
		for (int idx = 0; idx < m_categories->count(); idx++) {
			dynamic_cast<AbstractSettingsCategory *>
				(m_categories->widget(idx))->categoryApply();
		}
		NS_Application::instance()->nsOptionPersist();
	}
	if ((role == QDialogButtonBox::RejectRole) ||
	    (role == QDialogButtonBox::AcceptRole)) {
		hide();
	}
}


void NS_Settings::categorychanged_slot(const QString &currentText)
{
	m_headerlabel->setText(currentText);
}


void NS_Settings::addCategory(AbstractSettingsCategory *widget)
{
	m_categories->addWidget(widget);
	QListWidgetItem *listItem = new QListWidgetItem;
	listItem->setText(widget->categoryName());
	m_categorylist->addItem(listItem);
}
