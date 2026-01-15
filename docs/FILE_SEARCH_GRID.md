# File Search Grid View

This document describes the new grid-based file search results feature for Telegram Desktop.

## Overview

The File Search Grid View is a modern, visual way to browse file search results in Telegram chats and groups. It's particularly useful for searching compressed archives (ZIP, RAR, 7Z, etc.) in Telegram groups.

## Features

- **Grid Layout**: Search results displayed in a responsive grid instead of a list
- **Thumbnail Previews**: Shows file thumbnails for quick visual identification
- **Adjacent Message Context**: Displays small previews of previous/next messages with images
- **File Type Filtering**: Filter results by file type:
  - All files
  - Archives (.zip, .rar, .7z, .tar, .gz, .tgz, .bz2, .xz, .lzma, .cab, .iso, .dmg)
  - Documents (.pdf, .doc, .docx, .xls, .xlsx, .ppt, .pptx, .odt, .ods, .odp, .txt, .rtf)
  - Images (.jpg, .jpeg, .png, .gif, .webp, .bmp, .tiff, .svg, .ico)
  - Videos (.mp4, .avi, .mkv, .mov, .webm, .wmv, .flv, .m4v)
  - Audio (.mp3, .wav, .flac, .ogg, .m4a, .aac, .wma, .opus)
- **Archive Badge**: Visual indicator for archive files
- **Click Navigation**: Click any result to jump directly to the message
- **Multi-select**: Ctrl+Click to select multiple files

## UI Location

The File Search Grid option can be enabled in:

**Settings → Advanced → Experimental Features → File Search Grid View**

Or programmatically via `Main::SessionSettings`:
```cpp
session().settings().setFileSearchGridEnabled(true);
```

## Usage

### Basic Usage (Programmatic)

```cpp
#include "dialogs/ui/file_search_grid.h"

// Create the grid widget
auto grid = new Dialogs::Ui::FileSearchGridWidget(parent, controller);

// Set search results
grid->setResults(searchResults);

// Filter to show only archives
grid->setArchivesOnly(searchResults);

// Or use specific filter
grid->setFilter(Dialogs::Ui::FileTypeFilter::Archives);

// Connect to item clicked signal
grid->itemClicked() | rpl::start_with_next([=](not_null<HistoryItem*> item) {
    // Navigate to the message
    controller->showMessage(item);
}, lifetime);
```

### Configuration

```cpp
Dialogs::Ui::FileSearchGridConfig config;
config.columns = 3;           // Number of columns
config.cellWidth = 120;       // Cell width in pixels
config.cellHeight = 140;      // Cell height in pixels
config.cellSpacing = 8;       // Spacing between cells
config.thumbnailSize = 80;    // Thumbnail size in pixels
config.showAdjacentMessages = true;  // Show prev/next message previews
config.filter = Dialogs::Ui::FileTypeFilter::Archives;  // Default filter

grid->setConfig(config);
```

## API Reference

### FileSearchGridWidget

| Method | Description |
|--------|-------------|
| `setResults(results)` | Set search results to display |
| `setResultsFiltered(results, filter)` | Set results with a specific filter |
| `setArchivesOnly(results)` | Convenience method to show only archive files |
| `clearResults()` | Clear all results |
| `setConfig(config)` | Set grid configuration |
| `setFilter(filter)` | Change the current file type filter |
| `filter()` | Get current filter |
| `itemCount()` | Get number of items after filtering |
| `selectedItems()` | Get list of selected items |
| `clearSelection()` | Clear all selections |
| `itemClicked()` | Signal emitted when an item is clicked |
| `itemContextMenu()` | Signal emitted for context menu |

### FileTypeFilter Enum

| Value | Description |
|-------|-------------|
| `All` | Show all file types |
| `Archives` | ZIP, RAR, 7Z, TAR, GZ, etc. |
| `Documents` | PDF, DOC, XLS, PPT, etc. |
| `Images` | JPG, PNG, GIF, WEBP, etc. |
| `Videos` | MP4, AVI, MKV, MOV, etc. |
| `Audio` | MP3, WAV, FLAC, OGG, etc. |

### Static Helpers

| Method | Description |
|--------|-------------|
| `IsArchiveFile(item)` | Check if a HistoryItem is an archive file |
| `MatchesFilter(item, filter)` | Check if item matches the specified filter |

## Implementation Details

The grid widget is implemented in:
- `Telegram/SourceFiles/dialogs/ui/file_search_grid.h` - Header file
- `Telegram/SourceFiles/dialogs/ui/file_search_grid.cpp` - Implementation

The setting is stored in:
- `Telegram/SourceFiles/main/main_session_settings.h` - `fileSearchGridEnabled()`

## Future Enhancements

- Drag-and-drop support for batch file operations
- Custom sort options (by date, size, name)
- Preview panel for quick file inspection
- Keyboard navigation support
