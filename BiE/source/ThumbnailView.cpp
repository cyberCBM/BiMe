#include "ThumbnailView.h"

#define BAD_IMG_SZ	64

ThumbnailView::ThumbnailView(QWidget *parent) : QListView(parent)
{
	BimData::thumbsBackgroundColor = BimData::appSettings->value("backgroundThumbColor").value<QColor>();
	BimData::thumbsTextColor = BimData::appSettings->value("textThumbColor").value<QColor>();
	setThumbColors();
	BimData::thumbSpacing = BimData::appSettings->value("thumbSpacing").toInt();
	BimData::thumbPagesReadahead = BimData::appSettings->value("thumbPagesReadahead").toInt();
	BimData::thumbsLayout = BimData::appSettings->value("thumbLayout").toInt();
	thumbSize = BimData::appSettings->value("thumbsZoomVal").toInt();
	currentRow = 0;

	setViewMode(QListView::IconMode);
	setSelectionMode(QAbstractItemView::ExtendedSelection);
	setResizeMode(QListView::Adjust);
	setWordWrap(true);
	setDragEnabled(true);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setItemDelegate(new QItemDelegate);
	setUniformItemSizes(false);

	thumbViewModel = new QStandardItemModel(this);
	thumbViewModel->setSortRole(SortRole);
	setModel(thumbViewModel);

	connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(loadVisibleThumbs(int)));
	connect(this->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), 
				this, SLOT(handleSelectionChanged(QItemSelection)));
   	connect(this, SIGNAL(doubleClicked(const QModelIndex &)), 
				parent, SLOT(loadImagefromThumb(const QModelIndex &)));
	connect(this->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), 
				parent, SLOT(updateViewerImageBySelection(QItemSelection)));

	thumbsDir = new QDir();
	fileFilters = new QStringList;

	emptyImg.load(":/images/no_image.png");

	QTime time = QTime::currentTime();
	qsrand((uint)time.msec());
	mainWindow = parent;
	infoView = new InfoViewer(this);
}

void ThumbnailView::setThumbColors()
{
	QString bgColor = "background: rgb(%1, %2, %3); ";
	bgColor = bgColor.arg(BimData::thumbsBackgroundColor.red())
						.arg(BimData::thumbsBackgroundColor.green())
						.arg(BimData::thumbsBackgroundColor.blue());

	QString ss = "QListView { " + bgColor + "background-image: url("
					+ BimData::thumbsBackImage
					+ "); background-attachment: fixed; }";
   	setStyleSheet(ss);

	QPalette scrollBarOrigPal = verticalScrollBar()->palette();
	QPalette thumbViewOrigPal = palette();
	thumbViewOrigPal.setColor(QPalette::Text, BimData::thumbsTextColor);
	setPalette(thumbViewOrigPal);
	verticalScrollBar()->setPalette(scrollBarOrigPal);
}

void ThumbnailView::selectCurrentIndex()
{
	if (currentIndex.isValid() && thumbViewModel->rowCount() > 0)
	{
		scrollTo(currentIndex);
		setCurrentIndex(currentIndex);
	}
}

QString ThumbnailView::getSingleSelectionFilename()
{
	if (selectionModel()->selectedIndexes().size() == 1)
		return thumbViewModel->item(selectionModel()->selectedIndexes().first().row())->data(FileNameRole).toString();

	return("");
}

int ThumbnailView::getNextRow()
{
	if (currentRow == thumbViewModel->rowCount() - 1)
		return -1;

	return currentRow + 1;
}

int ThumbnailView::getPrevRow()
{
	if (currentRow == 0)
		return -1;

	return currentRow - 1;
}

int ThumbnailView::getLastRow()
{
	return thumbViewModel->rowCount() - 1;
}

int ThumbnailView::getRandomRow()
{
	return qrand() % (thumbViewModel->rowCount());
}

int ThumbnailView::getCurrentRow()
{
	return currentRow;
}

void ThumbnailView::setCurrentRow(int row)
{
	if (row >= 0)
		currentRow = row;
	else
		currentRow = 0;
}

void ThumbnailView::setImageviewWindowTitle()
{
	QString title = thumbViewModel->item(currentRow)->data(FileNameRole).toString()
			+ " - ["
			+ QString::number(currentRow + 1)
			+ "/"
			+ QString::number(thumbViewModel->rowCount())
            + "] - bim";


	mainWindow->setWindowTitle(title);
}

bool ThumbnailView::setCurrentIndexByName(QString &FileName)
{
	QModelIndexList indexList = thumbViewModel->match(thumbViewModel->index(0, 0), FileNameRole, FileName);
	if (indexList.size())
	{
	 	currentIndex = indexList[0];
	 	setCurrentRow(currentIndex.row());
		setRowHidden(currentIndex.row(), false);
	 	return true;
 	}

 	return false;
}

bool ThumbnailView::setCurrentIndexByRow(int row)
{
	QModelIndex idx = thumbViewModel->indexFromItem(thumbViewModel->item(row));
	if (idx.isValid())
	{
		currentIndex = idx;
	 	setCurrentRow(idx.row());
	 	return true;
 	}

 	return false;
}

void ThumbnailView::updateExifInfo(QString imageFullPath)
{
	Exiv2::Image::AutoPtr exifImage;
	QString key;
	QString val;

	try	{
		exifImage = Exiv2::ImageFactory::open(imageFullPath.toStdString());
		exifImage->readMetadata();
	}
	catch (Exiv2::Error &error) {
		return;
	}

	Exiv2::ExifData &exifData = exifImage->exifData();
	if (!exifData.empty()) {
		Exiv2::ExifData::const_iterator end = exifData.end();
		infoView->addTitleEntry("Exif");
		for (Exiv2::ExifData::const_iterator md = exifData.begin(); md != end; ++md) {
			// qDebug() << Exiv2::toString(md->key()).c_str() << " " << Exiv2::toString(md->value()).c_str();
			key = QString::fromUtf8(md->tagName().c_str());
			val = QString::fromUtf8(md->print().c_str());
			infoView->addEntry(key, val);
		}
	}

	Exiv2::IptcData &iptcData = exifImage->iptcData();
	if (!iptcData.empty()) {
		Exiv2::IptcData::iterator end = iptcData.end();
		infoView->addTitleEntry("IPTC");
		for (Exiv2::IptcData::iterator md = iptcData.begin(); md != end; ++md) {
			key = QString::fromUtf8(md->tagName().c_str());
			val = QString::fromUtf8(md->print().c_str());
	   		infoView->addEntry(key, val);
		}
	}

	Exiv2::XmpData &xmpData = exifImage->xmpData();
	if (!xmpData.empty()) {
		Exiv2::XmpData::iterator end = xmpData.end();
		infoView->addTitleEntry("XMP");
		for (Exiv2::XmpData::iterator md = xmpData.begin(); md != end; ++md) {
			key = QString::fromUtf8(md->tagName().c_str());
			val = QString::fromUtf8(md->print().c_str());
			infoView->addEntry(key, val);
		}
	}
}

void ThumbnailView::handleSelectionChanged(const QItemSelection&)
{
	QString info;
	QModelIndexList indexesList = selectionModel()->selectedIndexes();
	int nSelected = indexesList.size();
	QString imageFullPath;

	infoView->clear();

	if (nSelected == 1) {
		QString imageFullPath = thumbViewModel->item(indexesList.first().row())->data(FileNameRole).toString();
		imageInfoReader.setFileName(imageFullPath);
		QString key;
		QString val;

		QFileInfo imageInfo = QFileInfo(imageFullPath);
		infoView->addTitleEntry(tr("General"));

		key = tr("File name");
		val = imageInfo.fileName();
		infoView->addEntry(key, val);

		key = tr("Location");
		val = imageInfo.path();
		infoView->addEntry(key, val);

		key = tr("Size");
		val = QString::number(imageInfo.size() / 1024.0, 'f', 2) + "K";
		infoView->addEntry(key, val);

		key = tr("Modified");
		val = imageInfo.lastModified().toString(Qt::SystemLocaleShortDate);
		infoView->addEntry(key, val);

		if (imageInfoReader.size().isValid()) {
			key = tr("Format");
			val = imageInfoReader.format().toUpper();
			infoView->addEntry(key, val);

			key = tr("Resolution");
			val = QString::number(imageInfoReader.size().width())
					+ "x"
					+ QString::number(imageInfoReader.size().height());
			infoView->addEntry(key, val);

			key = tr("Megapixel");
			val = QString::number((imageInfoReader.size().width() * imageInfoReader.size().height()) / 1000000.0, 'f', 2);
			infoView->addEntry(key, val);

			updateExifInfo(imageFullPath);
			recentThumb = imageFullPath;
		} else {
			imageInfoReader.read();
			key = tr("Error");
			val = imageInfoReader.errorString();
			infoView->addEntry(key, val);
		}
	}
	updateThumbsSelection();
}

void ThumbnailView::startDrag(Qt::DropActions)
{
	QModelIndexList indexesList = selectionModel()->selectedIndexes();
	if (indexesList.isEmpty()) {
		return;
	}

	QDrag *drag = new QDrag(this);
	QMimeData *mimeData = new QMimeData;
	QList<QUrl> urls;
	for (QModelIndexList::const_iterator it = indexesList.constBegin(),
										end = indexesList.constEnd(); it != end; ++it)
	{
		urls << QUrl(thumbViewModel->item(it->row())->data(FileNameRole).toString());
	}
	mimeData->setUrls(urls);
	drag->setMimeData(mimeData);
	QPixmap pix;
	if (indexesList.count() > 1) {
		pix = QPixmap(128, 112);
		pix.fill(Qt::transparent);
		QPainter painter(&pix);
		painter.setBrush(Qt::NoBrush);
		painter.setPen(QPen(Qt::white, 2));
		int x = 0, y = 0, xMax = 0, yMax = 0;
		for (int i = 0; i < qMin(5, indexesList.count()); ++i) {
			QPixmap pix = thumbViewModel->item(indexesList.at(i).row())->icon().pixmap(72);
			if (i == 4) {
				x = (xMax - pix.width()) / 2;
				y = (yMax - pix.height()) / 2;
			}
			painter.drawPixmap(x, y, pix);
			xMax = qMax(xMax, qMin(128, x + pix.width()));
			yMax = qMax(yMax, qMin(112, y + pix.height()));
			painter.drawRect(x + 1, y + 1, qMin(126, pix.width() - 2), qMin(110, pix.height() - 2));
			x = !(x == y) * 56;
			y = !y * 40;
		}
		painter.end();
		pix = pix.copy(0, 0, xMax, yMax);
		drag->setPixmap(pix);
	} else {
		pix = thumbViewModel->item(indexesList.at(0).row())->icon().pixmap(128);
		drag->setPixmap(pix);
	}
	drag->setHotSpot(QPoint(pix.width() / 2, pix.height() / 2));
	drag->exec(Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, Qt::IgnoreAction);
}

void ThumbnailView::abort()
{
	abortOp = true;
}

void ThumbnailView::loadVisibleThumbs(int scrollBarValue)
{
	static int lastScrollBarValue = 0;

	if (BimData::thumbsLayout == Compact) {
		scrolledForward = true;
	} else {
		scrolledForward = (scrollBarValue >= lastScrollBarValue);
	}

	lastScrollBarValue = scrollBarValue;

Start:
	int first = getFirstVisibleThumb();
	int last = getLastVisibleThumb();
	if (abortOp || first < 0 || last < 0) 
		return;

	if (scrolledForward)
	{
		last += ((last - first) * (BimData::thumbPagesReadahead + 1));
		if (last >= thumbViewModel->rowCount())
			last = thumbViewModel->rowCount() - 1;
	}
	else
	{
		first -= (last - first) * (BimData::thumbPagesReadahead + 1);
		if (first < 0)
			first = 0;

		last += 10;
		if (last >= thumbViewModel->rowCount())
			last = thumbViewModel->rowCount() - 1;
	}

	if (thumbsRangeFirst == first && thumbsRangeLast == last) {
		return;
	}

	thumbsRangeFirst = first;
	thumbsRangeLast = last;

	loadThumbsRange();
	if (!abortOp)
		goto Start;
}

int ThumbnailView::getFirstVisibleThumb()
{
	QModelIndex idx;

	for (int currThumb = 0; currThumb < thumbViewModel->rowCount(); ++currThumb)
	{
		idx = thumbViewModel->indexFromItem(thumbViewModel->item(currThumb));
		if (viewport()->rect().contains(QPoint(0, visualRect(idx).y() + visualRect(idx).height() + 1)))
		{
			return idx.row();
		}
	}

	return -1;
}

int ThumbnailView::getLastVisibleThumb()
{
	QModelIndex idx;

	for (int currThumb = thumbViewModel->rowCount() -1; currThumb >= 0 ; --currThumb)
	{
		idx = thumbViewModel->indexFromItem(thumbViewModel->item(currThumb));
		if (viewport()->rect().contains(QPoint(0, visualRect(idx).y() + visualRect(idx).height() + 1)))
		{
			return idx.row();
		}
	}

	return -1;
}

bool ThumbnailView::isThumbVisible(QModelIndex idx)
{
	if (viewport()->rect().contains(QPoint(0, visualRect(idx).y() + visualRect(idx).height() + 1)))
	{
		return true;
	}

	return false;
}

void ThumbnailView::updateThumbsCount()
{
	QString state ;
	
	if (thumbViewModel->rowCount() > 0) {
		state = tr("%n image(s)", "", thumbViewModel->rowCount());
	} else {
		state = tr("No images");
	}

	emit setStatus(state);
}

void ThumbnailView::updateThumbsSelection()
{
	QString state;
	int nSelected = selectionModel()->selectedIndexes().size();

	if (!nSelected) {
		updateThumbsCount();
		return;
	} else if (nSelected >= 1)
		state = tr("Selected %1 of%2")
					.arg(QString::number(nSelected))
					.arg(tr(" %n image(s)", "", thumbViewModel->rowCount()));
	emit setStatus(state);
}

void ThumbnailView::loadPrepare()
{
	float thumbAspect = 1.33;
	if (BimData::thumbsLayout == Compact)
		thumbAspect = 1.77;
	else if (BimData::thumbsLayout == Squares)
		thumbAspect = 2;

	thumbHeight = (BimData::thumbsLayout == Squares)? thumbSize * thumbAspect : thumbSize;
	thumbWidth = (BimData::thumbsLayout == Squares)? thumbSize * thumbAspect : thumbHeight * thumbAspect;
	setIconSize(QSize(thumbWidth, thumbHeight));

	fileFilters->clear();
	QString textFilter("*");
	textFilter+= filterStr;
	*fileFilters	<< textFilter + "*.BMP"
					<< textFilter + "*.GIF"
					<< textFilter + "*.ICO"
					<< textFilter + "*.JPEG"
					<< textFilter + "*.JPG"
					<< textFilter + "*.MNG"
					<< textFilter + "*.PBM"
					<< textFilter + "*.PGM"
					<< textFilter + "*.PNG"
					<< textFilter + "*.PPM"
					<< textFilter + "*.SVG"
					<< textFilter + "*.SVGZ"
					<< textFilter + "*.TGA"					
					<< textFilter + "*.TIF"
					<< textFilter + "*.TIFF"
					<< textFilter + "*.WBMP"
					<< textFilter + "*.XBM"
					<< textFilter + "*.XPM"
					<< textFilter + "*.JPE";
	thumbsDir->setNameFilters(*fileFilters);
	thumbsDir->setFilter(QDir::Files);
	if (BimData::showHiddenFiles) {
		thumbsDir->setFilter(thumbsDir->filter() | QDir::Hidden);
	}
	
	thumbsDir->setPath(currentViewDir);

	QDir::SortFlags tempThumbsSortFlags = thumbsSortFlags;
	if (tempThumbsSortFlags & QDir::Size || tempThumbsSortFlags & QDir::Time) {
		tempThumbsSortFlags ^= QDir::Reversed;
	}
	thumbsDir->setSorting(tempThumbsSortFlags);

	thumbViewModel->clear();
	setSpacing(BimData::thumbSpacing);

	if (isNeedScroll) {
		scrollToTop();
	}

	abortOp = false;
	newIndex = 0;

	thumbsRangeFirst = -1;
	thumbsRangeLast = -1;
}

void ThumbnailView::load()
{
	loadPrepare();
	initThumbs();
	updateThumbsCount();
	loadVisibleThumbs();

	if (BimData::includeSubFolders) {
		emit showBusy(true);
		QDirIterator iterator(currentViewDir, QDirIterator::Subdirectories);
		while (iterator.hasNext()) {
			iterator.next();
			if (iterator.fileInfo().isDir() && iterator.fileName() != "." && iterator.fileName() != "..") {
				thumbsDir->setPath(iterator.filePath());

				initThumbs();
				updateThumbsCount();
				loadVisibleThumbs();

				if (abortOp) {
					goto finish;
				}
			}
			QApplication::processEvents();
		}
		updateThumbsSelection();
	}

finish:
	busy = false;
	emit showBusy(false);
	return;
}

void ThumbnailView::loadDuplicates()
{
	loadPrepare();

	emit showBusy(true);
	emit setStatus(tr("Searching duplicate images..."));

	dupImageHashes.clear();
	findDupes(true);

	if (BimData::includeSubFolders) {
		QDirIterator iterator(currentViewDir, QDirIterator::Subdirectories);
		while (iterator.hasNext()) {
			iterator.next();
			if (iterator.fileInfo().isDir() && iterator.fileName() != "." && iterator.fileName() != "..") {
				thumbsDir->setPath(iterator.filePath());

				findDupes(false);
				if (abortOp) {
					goto finish;
				}
			}
			QApplication::processEvents();
		}
	}

finish:
	busy = false;
	emit showBusy(false);
	return;
}

void ThumbnailView::initThumbs()
{
	thumbFileInfoList = thumbsDir->entryInfoList();
	static QStandardItem *thumbIitem;
	static int currThumb;
	static QPixmap emptyPixMap;
	static QSize hintSize;

	emptyPixMap = QPixmap::fromImage(emptyImg).scaled(thumbWidth, thumbHeight);

	if (BimData::thumbsLayout == Squares)
		hintSize = QSize(thumbWidth / 2, thumbWidth / 2);
	else if (BimData::thumbsLayout == Classic)
		hintSize = QSize(thumbWidth, thumbHeight + 
							(BimData::showLabels? QFontMetrics(font()).height() + 5 : 0));

	for (currThumb = 0; currThumb < thumbFileInfoList.size(); ++currThumb)
	{
		thumbFileInfo = thumbFileInfoList.at(currThumb);
		thumbIitem = new QStandardItem();
		thumbIitem->setData(false, LoadedRole);
		thumbIitem->setData(currThumb, SortRole);
		thumbIitem->setData(thumbFileInfo.filePath(), FileNameRole);
		if (BimData::thumbsLayout != Squares && BimData::showLabels)
			thumbIitem->setData(thumbFileInfo.fileName(), Qt::DisplayRole);
		if (BimData::thumbsLayout == Compact)
			thumbIitem->setIcon(emptyPixMap);
		thumbIitem->setTextAlignment(Qt::AlignTop | Qt::AlignHCenter);
		if (BimData::thumbsLayout != Compact)
			thumbIitem->setSizeHint(hintSize);

		thumbViewModel->appendRow(thumbIitem);
	}
}

void ThumbnailView::updateFoundDupesState(int duplicates, int filesScanned, int originalImages)
{
	QString state;
	state = tr("Scanned %1, displaying %2 (%3 and %4)")
				.arg(tr("%n image(s)", "", filesScanned))
				.arg(tr("%n image(s)", "", originalImages + duplicates))
				.arg(tr("%n original(s)", "", originalImages))
				.arg(tr("%n duplicate(s)", "", duplicates));
	emit setStatus(state);
}

void ThumbnailView::findDupes(bool resetCounters)
{
	thumbFileInfoList = thumbsDir->entryInfoList();
	static int originalImages;
	static int foundDups;
	static int totalFiles;
	if (resetCounters) {
		originalImages = totalFiles = foundDups = 0;
	}

	for (int currThumb = 0; currThumb < thumbFileInfoList.size(); ++currThumb) {
		thumbFileInfo = thumbFileInfoList.at(currThumb);
	    QCryptographicHash md5gen(QCryptographicHash::Md5);
	    QString currentFilePath = thumbFileInfo.filePath();

	    QFile file(currentFilePath);
		if (!file.open(QIODevice::ReadOnly)) {
		    continue;
		}
		totalFiles++;

	    md5gen.addData(file.readAll());
	    file.close();
		QString md5 = md5gen.result().toHex();

		if (dupImageHashes.contains(md5)) {
			if (dupImageHashes[md5].duplicates < 1) {
				addThumb(dupImageHashes[md5].filePath);
				originalImages++;
			}

			foundDups++;
			dupImageHashes[md5].duplicates++;
			addThumb(currentFilePath);
		} else {
			DuplicateImage dupImage;
			dupImage.filePath = currentFilePath; 
			dupImage.duplicates = 0; 
			dupImageHashes.insert(md5, dupImage);
		}

		
		QApplication::processEvents();
		updateFoundDupesState(foundDups, totalFiles, originalImages);
		
		if (abortOp) {
			break;
		}
	}
	updateFoundDupesState(foundDups, totalFiles, originalImages);
}

void ThumbnailView::loadThumbsRange()
{
	static bool inProgress = false;
	static QImageReader thumbReader;
	static QSize currThumbSize;
	static int currRowCount;
	static QString imageFileName;
	static QImage thumb;
	int currThumb;

	if (inProgress) {	
		abortOp = true;
		QTimer::singleShot(0, this, SLOT(loadThumbsRange()));
		return;
	}

	inProgress = true;
	currRowCount = thumbViewModel->rowCount();

	for (	scrolledForward? currThumb = thumbsRangeFirst : currThumb = thumbsRangeLast;
			(scrolledForward? currThumb <= thumbsRangeLast : currThumb >= thumbsRangeFirst);
			scrolledForward? ++currThumb : --currThumb) {
		if (abortOp || thumbViewModel->rowCount() != currRowCount)
			break;

		if (thumbViewModel->item(currThumb)->data(LoadedRole).toBool())
			continue;

		imageFileName = thumbViewModel->item(currThumb)->data(FileNameRole).toString();
		thumbReader.setFileName(imageFileName);
		currThumbSize = thumbReader.size();

		if (currThumbSize.isValid()) {
			if (!BimData::noEnlargeSmallThumb
				|| (currThumbSize.width() > thumbWidth || currThumbSize.height() > thumbHeight)) {
				currThumbSize.scale(QSize(thumbWidth, thumbHeight), Qt::KeepAspectRatio);
			}

			thumbReader.setScaledSize(currThumbSize);
			thumb = thumbReader.read();

			if (BimData::exifThumbRotationEnabled) {
				ImageViewer::rotateByExifRotation(thumb, imageFileName);
				currThumbSize = thumb.size();
				currThumbSize.scale(QSize(thumbWidth, thumbHeight), Qt::KeepAspectRatio);
			}
			thumbViewModel->item(currThumb)->setIcon(QPixmap::fromImage(thumb));
		} else {
			thumbViewModel->item(currThumb)->setIcon(QIcon::fromTheme("image-missing",
									QIcon(":/images/error_image.png")).pixmap(BAD_IMG_SZ, BAD_IMG_SZ));
			currThumbSize.setHeight(BAD_IMG_SZ);
			currThumbSize.setWidth(BAD_IMG_SZ);
		}

		if (BimData::thumbsLayout == Compact) {
			if (BimData::showLabels)
				currThumbSize.setHeight(currThumbSize.height() + QFontMetrics(font()).height() + 5);
			thumbViewModel->item(currThumb)->setSizeHint(currThumbSize);
			if (isThumbVisible(thumbViewModel->item(currThumb)->index()))
				setRowHidden(currThumb, false);
		}

		thumbViewModel->item(currThumb)->setData(true, LoadedRole);

		QApplication::processEvents();
	}

	if (BimData::thumbsLayout == Compact && thumbViewModel->rowCount() > 0) {
		setRowHidden(0 , false);
	}

	inProgress = false;
	abortOp = false;
}

void ThumbnailView::addThumb(QString &imageFullPath)
{
	QStandardItem *thumbIitem = new QStandardItem();
	QImageReader thumbReader;
	QSize hintSize;
	QSize currThumbSize;
	static QImage thumb;

	if (BimData::thumbsLayout == Squares)
		hintSize = QSize(thumbWidth / 2, thumbWidth / 2);
	else if (BimData::thumbsLayout == Classic)
		hintSize = QSize(thumbWidth, thumbHeight + 
							(BimData::showLabels? QFontMetrics(font()).height() + 5 : 0));
	
	thumbFileInfo = QFileInfo(imageFullPath);
	thumbIitem->setData(true, LoadedRole);
	thumbIitem->setData(0, SortRole);
	thumbIitem->setData(thumbFileInfo.filePath(), FileNameRole);
	if (BimData::thumbsLayout != Squares && BimData::showLabels)
		thumbIitem->setData(thumbFileInfo.fileName(), Qt::DisplayRole);

	thumbReader.setFileName(imageFullPath);
	currThumbSize = thumbReader.size();
	if (currThumbSize.isValid())
	{
		if (!BimData::noEnlargeSmallThumb
			|| (currThumbSize.width() > thumbWidth || currThumbSize.height() > thumbHeight))
		{
			currThumbSize.scale(QSize(thumbWidth, thumbHeight), Qt::KeepAspectRatio);
		}
			
		thumbReader.setScaledSize(currThumbSize);
		thumb = thumbReader.read();

		if (BimData::exifThumbRotationEnabled)
		{
			ImageViewer::rotateByExifRotation(thumb, imageFullPath);
			currThumbSize = thumb.size();
			currThumbSize.scale(QSize(thumbWidth, thumbHeight), Qt::KeepAspectRatio);
		}
			
		thumbIitem->setIcon(QPixmap::fromImage(thumb));
	}
	else
	{
		thumbIitem->setIcon(QIcon::fromTheme("image-missing",
								QIcon(":/images/error_image.png")).pixmap(BAD_IMG_SZ, BAD_IMG_SZ));
		currThumbSize.setHeight(BAD_IMG_SZ);
		currThumbSize.setWidth(BAD_IMG_SZ);
	}

	if (BimData::thumbsLayout == Compact)
	{
		if (BimData::showLabels)
			currThumbSize.setHeight(currThumbSize.height() + QFontMetrics(font()).height() + 5);
		thumbIitem->setSizeHint(currThumbSize);
	}
	else
		thumbIitem->setSizeHint(hintSize);

	thumbViewModel->appendRow(thumbIitem);
}

void ThumbnailView::wheelEvent(QWheelEvent *event)
{
	if (event->delta() < 0)
		verticalScrollBar()->setValue(verticalScrollBar()->value() + thumbHeight);
	else
		verticalScrollBar()->setValue(verticalScrollBar()->value() - thumbHeight);
}

void ThumbnailView::mousePressEvent(QMouseEvent *event)
{
	QListView::mousePressEvent(event);

	if (BimData::reverseMouseBehavior && event->button() == Qt::MiddleButton)
	{
		if (selectionModel()->selectedIndexes().size() == 1)
			emit(doubleClicked(selectionModel()->selectedIndexes().first()));
	}
}

void ThumbnailView::invertSelection()
{
	QItemSelection toggleSelection;
	QModelIndex firstIndex = thumbViewModel->index(0, 0);
	QModelIndex lastIndex = thumbViewModel->index(thumbViewModel->rowCount() - 1, 0);
	toggleSelection.select(firstIndex, lastIndex);
	selectionModel()->select(toggleSelection, QItemSelectionModel::Toggle);
}

