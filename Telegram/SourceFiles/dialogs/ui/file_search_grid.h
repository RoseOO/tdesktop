/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"
#include "ui/effects/animations.h"
#include "base/timer.h"

class HistoryItem;

namespace Data {
class DocumentMedia;
class PhotoMedia;
} // namespace Data

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace Dialogs::Ui {

// Forward declarations
struct FileSearchGridItem;

// File type filter options
enum class FileTypeFilter {
	All,
	Archives,      // .zip, .rar, .7z, .tar, .gz, .tgz, .bz2
	Documents,     // .pdf, .doc, .docx, .xls, .xlsx, .ppt, .pptx
	Images,        // .jpg, .jpeg, .png, .gif, .webp, .bmp
	Videos,        // .mp4, .avi, .mkv, .mov, .webm
	Audio,         // .mp3, .wav, .flac, .ogg, .m4a
};

// Configuration for the grid display
struct FileSearchGridConfig {
	int columns = 3;
	int cellWidth = 120;
	int cellHeight = 140;
	int cellSpacing = 8;
	int thumbnailSize = 80;
	bool showAdjacentMessages = true;
	FileTypeFilter filter = FileTypeFilter::All;
};

// Represents a single item in the search results grid
struct FileSearchGridItem {
	not_null<HistoryItem*> item;
	HistoryItem* prevItem = nullptr;
	HistoryItem* nextItem = nullptr;

	// Cached thumbnail data
	QImage thumbnail;
	QImage prevThumbnail;
	QImage nextThumbnail;

	// Item metadata
	QString fileName;
	QString fileSize;
	QString date;
	bool isArchive = false;

	// State
	bool selected = false;
	bool hovered = false;
};

// Grid widget for displaying file search results
class FileSearchGridWidget final : public ::Ui::RpWidget {
public:
	FileSearchGridWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller);
	~FileSearchGridWidget();

	// Set search results to display
	void setResults(std::vector<not_null<HistoryItem*>> results);

	// Set results with filtering (only shows files matching the filter)
	void setResultsFiltered(
		std::vector<not_null<HistoryItem*>> results,
		FileTypeFilter filter);

	// Filter to show only archive files
	void setArchivesOnly(std::vector<not_null<HistoryItem*>> results);

	// Clear all results
	void clearResults();

	// Configuration
	void setConfig(const FileSearchGridConfig &config);
	[[nodiscard]] const FileSearchGridConfig &config() const;

	// Set file type filter
	void setFilter(FileTypeFilter filter);
	[[nodiscard]] FileTypeFilter filter() const;

	// Get count of items (after filtering)
	[[nodiscard]] int itemCount() const;

	// Selection
	[[nodiscard]] std::vector<not_null<HistoryItem*>> selectedItems() const;
	void clearSelection();

	// Signals
	[[nodiscard]] rpl::producer<not_null<HistoryItem*>> itemClicked() const;
	[[nodiscard]] rpl::producer<not_null<HistoryItem*>> itemContextMenu() const;

	// Static helper to check if item is an archive file
	[[nodiscard]] static bool IsArchiveFile(not_null<HistoryItem*> item);
	[[nodiscard]] static bool MatchesFilter(
		not_null<HistoryItem*> item,
		FileTypeFilter filter);

protected:
	void paintEvent(QPaintEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;
	void leaveEvent(QEvent *e) override;
	int resizeGetHeight(int newWidth) override;

private:
	void updateHovered(QPoint pos);
	void paintItem(
		QPainter &p,
		const FileSearchGridItem &item,
		const QRect &rect);
	void paintThumbnail(
		QPainter &p,
		const QImage &thumbnail,
		const QRect &rect,
		bool isMain);
	void paintAdjacentPreview(
		QPainter &p,
		const QImage &thumbnail,
		const QRect &rect,
		bool isLeft);
	void paintFileInfo(
		QPainter &p,
		const FileSearchGridItem &item,
		const QRect &rect);
	void paintArchiveBadge(
		QPainter &p,
		const QRect &rect);

	[[nodiscard]] int itemIndexAt(QPoint pos) const;
	[[nodiscard]] QRect itemRect(int index) const;
	[[nodiscard]] int calculateHeight() const;

	void loadThumbnails();
	void loadItemThumbnail(FileSearchGridItem &item);
	void applyFilter();

	const not_null<Window::SessionController*> _controller;
	FileSearchGridConfig _config;
	std::vector<not_null<HistoryItem*>> _allResults;
	std::vector<FileSearchGridItem> _items;
	int _hoveredIndex = -1;
	int _pressedIndex = -1;

	rpl::event_stream<not_null<HistoryItem*>> _itemClicked;
	rpl::event_stream<not_null<HistoryItem*>> _itemContextMenu;

	base::Timer _thumbnailLoadTimer;
};

} // namespace Dialogs::Ui
