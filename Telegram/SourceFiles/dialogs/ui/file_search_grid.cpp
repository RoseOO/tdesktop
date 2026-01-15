/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "dialogs/ui/file_search_grid.h"

#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_media_types.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "history/history.h"
#include "main/main_session.h"
#include "ui/image/image.h"
#include "ui/painter.h"
#include "ui/text/format_values.h"
#include "ui/cached_round_corners.h"
#include "window/window_session_controller.h"
#include "styles/style_dialogs.h"
#include "styles/style_overview.h"

#include <QDateTime>

namespace Dialogs::Ui {
namespace {

constexpr auto kThumbnailLoadDelay = crl::time(100);
constexpr auto kCornerRadius = 8;
constexpr auto kAdjacentPreviewSize = 24;
constexpr auto kAdjacentPreviewOffset = 4;

[[nodiscard]] QString FormatFileSize(int64 size) {
	return ::Ui::FormatSizeText(size);
}

[[nodiscard]] QString FormatDate(TimeId date) {
	return QDateTime::fromSecsSinceEpoch(date).toString("MMM d, yyyy");
}

[[nodiscard]] QImage CreateRoundedThumbnail(
		const QImage &source,
		int size,
		int radius) {
	if (source.isNull()) {
		return QImage();
	}

	auto result = source.scaled(
		size,
		size,
		Qt::KeepAspectRatioByExpanding,
		Qt::SmoothTransformation);

	// Center crop if needed
	if (result.width() > size || result.height() > size) {
		const auto x = (result.width() - size) / 2;
		const auto y = (result.height() - size) / 2;
		result = result.copy(x, y, size, size);
	}

	// Apply rounded corners
	QImage rounded(size, size, QImage::Format_ARGB32_Premultiplied);
	rounded.fill(Qt::transparent);

	{
		QPainter p(&rounded);
		p.setRenderHint(QPainter::Antialiasing);
		p.setRenderHint(QPainter::SmoothPixmapTransform);

		QPainterPath path;
		path.addRoundedRect(QRectF(0, 0, size, size), radius, radius);
		p.setClipPath(path);
		p.drawImage(0, 0, result);
	}

	return rounded;
}

[[nodiscard]] QImage GetDocumentThumbnail(not_null<HistoryItem*> item) {
	if (const auto media = item->media()) {
		if (const auto document = media->document()) {
			if (const auto thumb = document->thumbnail()) {
				return thumb->original();
			}
		}
		if (const auto photo = media->photo()) {
			if (const auto thumb = photo->thumbnail()) {
				return thumb->original();
			}
		}
	}
	return QImage();
}

[[nodiscard]] QString GetFileName(not_null<HistoryItem*> item) {
	if (const auto media = item->media()) {
		if (const auto document = media->document()) {
			const auto name = document->filename();
			return name.isEmpty() ? "File" : name;
		}
	}
	return "File";
}

[[nodiscard]] int64 GetFileSize(not_null<HistoryItem*> item) {
	if (const auto media = item->media()) {
		if (const auto document = media->document()) {
			return document->size;
		}
	}
	return 0;
}

[[nodiscard]] bool IsArchiveFile(not_null<HistoryItem*> item) {
	if (const auto media = item->media()) {
		if (const auto document = media->document()) {
			const auto name = document->filename().toLower();
			return name.endsWith(".zip")
				|| name.endsWith(".rar")
				|| name.endsWith(".7z")
				|| name.endsWith(".tar")
				|| name.endsWith(".gz")
				|| name.endsWith(".tar.gz")
				|| name.endsWith(".tgz")
				|| name.endsWith(".bz2");
		}
	}
	return false;
}

} // namespace

FileSearchGridWidget::FileSearchGridWidget(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: RpWidget(parent)
, _controller(controller)
, _thumbnailLoadTimer([=] { loadThumbnails(); }) {
	setMouseTracking(true);
}

FileSearchGridWidget::~FileSearchGridWidget() = default;

void FileSearchGridWidget::setResults(
		std::vector<not_null<HistoryItem*>> results) {
	_items.clear();
	_items.reserve(results.size());

	for (size_t i = 0; i < results.size(); ++i) {
		FileSearchGridItem gridItem;
		gridItem.item = results[i];

		// Set adjacent items for context
		if (i > 0) {
			gridItem.prevItem = results[i - 1];
		}
		if (i + 1 < results.size()) {
			gridItem.nextItem = results[i + 1];
		}

		// Extract file metadata
		gridItem.fileName = GetFileName(results[i]);
		gridItem.fileSize = FormatFileSize(GetFileSize(results[i]));
		gridItem.date = FormatDate(results[i]->date());

		_items.push_back(std::move(gridItem));
	}

	_hoveredIndex = -1;
	_pressedIndex = -1;

	// Schedule thumbnail loading
	_thumbnailLoadTimer.callOnce(kThumbnailLoadDelay);

	resize(width(), calculateHeight());
	update();
}

void FileSearchGridWidget::clearResults() {
	_items.clear();
	_hoveredIndex = -1;
	_pressedIndex = -1;
	resize(width(), 0);
	update();
}

void FileSearchGridWidget::setConfig(const FileSearchGridConfig &config) {
	_config = config;
	resize(width(), calculateHeight());
	update();
}

const FileSearchGridConfig &FileSearchGridWidget::config() const {
	return _config;
}

std::vector<not_null<HistoryItem*>> FileSearchGridWidget::selectedItems() const {
	std::vector<not_null<HistoryItem*>> result;
	for (const auto &item : _items) {
		if (item.selected) {
			result.push_back(item.item);
		}
	}
	return result;
}

void FileSearchGridWidget::clearSelection() {
	for (auto &item : _items) {
		item.selected = false;
	}
	update();
}

rpl::producer<not_null<HistoryItem*>> FileSearchGridWidget::itemClicked() const {
	return _itemClicked.events();
}

rpl::producer<not_null<HistoryItem*>> FileSearchGridWidget::itemContextMenu() const {
	return _itemContextMenu.events();
}

void FileSearchGridWidget::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	p.setRenderHint(QPainter::Antialiasing);
	p.setRenderHint(QPainter::SmoothPixmapTransform);

	const auto clip = e->rect();

	for (int i = 0; i < static_cast<int>(_items.size()); ++i) {
		const auto rect = itemRect(i);
		if (!rect.intersects(clip)) {
			continue;
		}

		auto &item = _items[i];
		item.hovered = (i == _hoveredIndex);
		paintItem(p, item, rect);
	}
}

void FileSearchGridWidget::paintItem(
		QPainter &p,
		const FileSearchGridItem &item,
		const QRect &rect) {
	const auto selected = item.selected;
	const auto hovered = item.hovered;

	// Draw background
	auto bgColor = selected
		? st::dialogsBgActive
		: hovered
		? st::dialogsBgOver
		: st::dialogsBg;
	
	{
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(bgColor);
		p.drawRoundedRect(rect, kCornerRadius, kCornerRadius);
	}

	// Draw border for selected items
	if (selected || hovered) {
		auto borderColor = selected
			? st::dialogsTextFgActive
			: st::dialogsTextFgOver;
		p.setPen(QPen(borderColor, 2));
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(rect.adjusted(1, 1, -1, -1), kCornerRadius - 1, kCornerRadius - 1);
	}

	// Calculate layout regions
	const auto padding = 8;
	const auto thumbnailRect = QRect(
		rect.x() + (rect.width() - _config.thumbnailSize) / 2,
		rect.y() + padding,
		_config.thumbnailSize,
		_config.thumbnailSize);

	// Paint main thumbnail
	paintThumbnail(p, item.thumbnail, thumbnailRect, true);

	// Paint adjacent message previews (if enabled and available)
	if (_config.showAdjacentMessages) {
		// Previous message preview (left side)
		if (!item.prevThumbnail.isNull()) {
			const auto prevRect = QRect(
				rect.x() + kAdjacentPreviewOffset,
				rect.y() + padding + kAdjacentPreviewOffset,
				kAdjacentPreviewSize,
				kAdjacentPreviewSize);
			paintAdjacentPreview(p, item.prevThumbnail, prevRect, true);
		}

		// Next message preview (right side)
		if (!item.nextThumbnail.isNull()) {
			const auto nextRect = QRect(
				rect.right() - kAdjacentPreviewSize - kAdjacentPreviewOffset,
				rect.y() + padding + kAdjacentPreviewOffset,
				kAdjacentPreviewSize,
				kAdjacentPreviewSize);
			paintAdjacentPreview(p, item.nextThumbnail, nextRect, false);
		}
	}

	// Paint file info at bottom
	const auto infoRect = QRect(
		rect.x() + padding,
		thumbnailRect.bottom() + padding / 2,
		rect.width() - 2 * padding,
		rect.height() - thumbnailRect.height() - padding * 2);
	paintFileInfo(p, item, infoRect);
}

void FileSearchGridWidget::paintThumbnail(
		QPainter &p,
		const QImage &thumbnail,
		const QRect &rect,
		bool isMain) {
	auto hq = PainterHighQualityEnabler(p);

	if (!thumbnail.isNull()) {
		p.drawImage(rect, thumbnail);
	} else {
		// Draw placeholder
		p.setPen(Qt::NoPen);
		p.setBrush(st::dialogsTextFg->c.lighter(200));
		p.drawRoundedRect(rect, kCornerRadius / 2, kCornerRadius / 2);

		// Draw file icon placeholder
		const auto iconSize = rect.width() / 2;
		const auto iconRect = QRect(
			rect.center().x() - iconSize / 2,
			rect.center().y() - iconSize / 2,
			iconSize,
			iconSize);

		p.setPen(QPen(st::dialogsTextFg, 2));
		p.setBrush(Qt::NoBrush);
		p.drawRect(iconRect);

		// Draw fold corner
		const auto foldSize = iconSize / 4;
		QPainterPath foldPath;
		foldPath.moveTo(iconRect.right() - foldSize, iconRect.top());
		foldPath.lineTo(iconRect.right(), iconRect.top() + foldSize);
		foldPath.lineTo(iconRect.right() - foldSize, iconRect.top() + foldSize);
		foldPath.closeSubpath();
		p.fillPath(foldPath, st::dialogsTextFg);
	}
}

void FileSearchGridWidget::paintAdjacentPreview(
		QPainter &p,
		const QImage &thumbnail,
		const QRect &rect,
		bool isLeft) {
	auto hq = PainterHighQualityEnabler(p);

	// Draw shadow/border for context
	p.setPen(QPen(QColor(0, 0, 0, 50), 1));
	p.setBrush(QColor(255, 255, 255, 200));
	p.drawRoundedRect(rect.adjusted(-1, -1, 1, 1), 4, 4);

	// Draw the thumbnail
	if (!thumbnail.isNull()) {
		p.drawImage(rect, thumbnail);
	} else {
		// Simple placeholder for images
		p.setPen(Qt::NoPen);
		p.setBrush(st::dialogsTextFg->c.lighter(180));
		p.drawRoundedRect(rect, 3, 3);
	}

	// Draw direction indicator
	p.setPen(QPen(QColor(0, 0, 0, 100), 1.5));
	const auto arrowSize = 4;
	const auto arrowY = rect.center().y();

	if (isLeft) {
		// Left arrow indicator
		const auto arrowX = rect.left() - 2;
		p.drawLine(arrowX, arrowY - arrowSize, arrowX - arrowSize, arrowY);
		p.drawLine(arrowX - arrowSize, arrowY, arrowX, arrowY + arrowSize);
	} else {
		// Right arrow indicator
		const auto arrowX = rect.right() + 2;
		p.drawLine(arrowX, arrowY - arrowSize, arrowX + arrowSize, arrowY);
		p.drawLine(arrowX + arrowSize, arrowY, arrowX, arrowY + arrowSize);
	}
}

void FileSearchGridWidget::paintFileInfo(
		QPainter &p,
		const FileSearchGridItem &item,
		const QRect &rect) {
	const auto selected = item.selected;
	const auto hovered = item.hovered;

	// File name (truncated with ellipsis)
	auto nameColor = selected
		? st::dialogsNameFgActive
		: hovered
		? st::dialogsNameFgOver
		: st::dialogsNameFg;
	p.setPen(nameColor);
	p.setFont(st::semiboldFont);

	const auto nameText = p.fontMetrics().elidedText(
		item.fileName,
		Qt::ElideMiddle,
		rect.width());
	p.drawText(rect.x(), rect.y() + st::semiboldFont->ascent, nameText);

	// File size and date
	auto infoColor = selected
		? st::dialogsTextFgActive
		: hovered
		? st::dialogsTextFgOver
		: st::dialogsTextFg;
	p.setPen(infoColor);
	p.setFont(st::normalFont);

	const auto infoText = item.fileSize + " • " + item.date;
	const auto elidedInfo = p.fontMetrics().elidedText(
		infoText,
		Qt::ElideRight,
		rect.width());
	p.drawText(
		rect.x(),
		rect.y() + st::semiboldFont->height + st::normalFont->ascent + 2,
		elidedInfo);
}

void FileSearchGridWidget::mouseMoveEvent(QMouseEvent *e) {
	updateHovered(e->pos());
}

void FileSearchGridWidget::mousePressEvent(QMouseEvent *e) {
	_pressedIndex = itemIndexAt(e->pos());
	update();
}

void FileSearchGridWidget::mouseReleaseEvent(QMouseEvent *e) {
	const auto index = itemIndexAt(e->pos());
	if (index == _pressedIndex && index >= 0 && index < static_cast<int>(_items.size())) {
		if (e->button() == Qt::LeftButton) {
			if (e->modifiers() & Qt::ControlModifier) {
				// Toggle selection with Ctrl+Click
				_items[index].selected = !_items[index].selected;
				update();
			} else {
				// Regular click - navigate to message
				_itemClicked.fire_copy(_items[index].item);
			}
		}
	}
	_pressedIndex = -1;
	update();
}

void FileSearchGridWidget::contextMenuEvent(QContextMenuEvent *e) {
	const auto index = itemIndexAt(e->pos());
	if (index >= 0 && index < static_cast<int>(_items.size())) {
		_itemContextMenu.fire_copy(_items[index].item);
	}
}

void FileSearchGridWidget::leaveEvent(QEvent *e) {
	_hoveredIndex = -1;
	update();
}

int FileSearchGridWidget::resizeGetHeight(int newWidth) {
	return calculateHeight();
}

void FileSearchGridWidget::updateHovered(QPoint pos) {
	const auto newHovered = itemIndexAt(pos);
	if (_hoveredIndex != newHovered) {
		_hoveredIndex = newHovered;
		update();
	}
}

int FileSearchGridWidget::itemIndexAt(QPoint pos) const {
	if (_items.empty()) {
		return -1;
	}

	const auto columns = std::max(1, _config.columns);
	const auto cellWidth = _config.cellWidth + _config.cellSpacing;
	const auto cellHeight = _config.cellHeight + _config.cellSpacing;

	const auto col = pos.x() / cellWidth;
	const auto row = pos.y() / cellHeight;

	if (col < 0 || col >= columns) {
		return -1;
	}

	const auto index = row * columns + col;
	if (index < 0 || index >= static_cast<int>(_items.size())) {
		return -1;
	}

	// Check if actually within the cell (not in spacing)
	const auto cellRect = itemRect(index);
	if (!cellRect.contains(pos)) {
		return -1;
	}

	return index;
}

QRect FileSearchGridWidget::itemRect(int index) const {
	if (index < 0 || index >= static_cast<int>(_items.size())) {
		return QRect();
	}

	const auto columns = std::max(1, _config.columns);
	const auto row = index / columns;
	const auto col = index % columns;

	const auto x = col * (_config.cellWidth + _config.cellSpacing);
	const auto y = row * (_config.cellHeight + _config.cellSpacing);

	return QRect(x, y, _config.cellWidth, _config.cellHeight);
}

int FileSearchGridWidget::calculateHeight() const {
	if (_items.empty()) {
		return 0;
	}

	const auto columns = std::max(1, _config.columns);
	const auto rows = (_items.size() + columns - 1) / columns;

	return rows * (_config.cellHeight + _config.cellSpacing);
}

void FileSearchGridWidget::loadThumbnails() {
	for (auto &item : _items) {
		loadItemThumbnail(item);
	}
	update();
}

void FileSearchGridWidget::loadItemThumbnail(FileSearchGridItem &item) {
	// Load main thumbnail
	auto mainThumb = GetDocumentThumbnail(item.item);
	if (!mainThumb.isNull()) {
		item.thumbnail = CreateRoundedThumbnail(
			mainThumb,
			_config.thumbnailSize,
			kCornerRadius / 2);
	}

	// Load adjacent thumbnails if enabled
	if (_config.showAdjacentMessages) {
		if (item.prevItem) {
			auto prevThumb = GetDocumentThumbnail(item.prevItem);
			if (!prevThumb.isNull()) {
				item.prevThumbnail = CreateRoundedThumbnail(
					prevThumb,
					kAdjacentPreviewSize,
					3);
			}
		}

		if (item.nextItem) {
			auto nextThumb = GetDocumentThumbnail(item.nextItem);
			if (!nextThumb.isNull()) {
				item.nextThumbnail = CreateRoundedThumbnail(
					nextThumb,
					kAdjacentPreviewSize,
					3);
			}
		}
	}
}

} // namespace Dialogs::Ui
