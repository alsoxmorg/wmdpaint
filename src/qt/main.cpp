#include <QApplication>
#include <QActionGroup>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDataStream>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QStatusBar>
#include <QToolBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr int kDefaultWidth = 512;
constexpr int kDefaultHeight = 512;
constexpr quint64 kMaxPixels = 100000000;

QImage newImage(int width, int height)
{
    QImage image(width, height, QImage::Format_RGBA64);
    image.fill(Qt::transparent);
    return image;
}

QImage loadFarbfeld(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return {};
    }
    if (file.read(8) != QByteArrayLiteral("farbfeld")) {
        *error = QStringLiteral("Not a farbfeld image");
        return {};
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    quint32 width = 0, height = 0;
    stream >> width >> height;
    const quint64 pixels = quint64(width) * height;
    if (!width || !height || pixels > kMaxPixels ||
        width > quint32(std::numeric_limits<int>::max()) ||
        height > quint32(std::numeric_limits<int>::max())) {
        *error = QStringLiteral("Invalid or unreasonably large farbfeld dimensions");
        return {};
    }

    QImage image(int(width), int(height), QImage::Format_RGBA64);
    for (int y = 0; y < image.height(); ++y) {
        auto *line = reinterpret_cast<QRgba64 *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            quint16 r, g, b, a;
            stream >> r >> g >> b >> a;
            if (stream.status() != QDataStream::Ok) {
                *error = QStringLiteral("Truncated farbfeld pixel data");
                return {};
            }
            line[x] = QRgba64::fromRgba64(r, g, b, a);
        }
    }
    return image;
}

bool saveFarbfeld(const QString &path, const QImage &source, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = file.errorString();
        return false;
    }
    if (file.write("farbfeld", 8) != 8) {
        *error = file.errorString();
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << quint32(source.width()) << quint32(source.height());
    const QImage image = source.convertToFormat(QImage::Format_RGBA64);
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgba64 *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgba64 pixel = line[x];
            stream << pixel.red() << pixel.green() << pixel.blue() << pixel.alpha();
        }
    }
    if (stream.status() != QDataStream::Ok) {
        *error = file.errorString().isEmpty() ? QStringLiteral("Write failed") : file.errorString();
        return false;
    }
    return true;
}

class Canvas final : public QWidget
{
    Q_OBJECT

public:
    enum class Tool { Pencil, Eraser };

    explicit Canvas(QWidget *parent = nullptr) : QWidget(parent), image_(newImage(kDefaultWidth, kDefaultHeight))
    {
        setAttribute(Qt::WA_StaticContents);
        setMouseTracking(true);
        updateSize();
    }

    const QImage &image() const { return image_; }
    void setImage(QImage image)
    {
        image_ = image.convertToFormat(QImage::Format_RGBA64);
        updateSize();
        update();
    }
    void setTool(Tool tool) { tool_ = tool; }
    void setColor(const QColor &color) { color_ = color; }
    qreal zoom() const { return zoom_; }
    void setZoom(qreal zoom)
    {
        zoom_ = std::clamp(zoom, 0.125, 64.0);
        updateSize();
        update();
        emit zoomChanged(zoom_);
    }

signals:
    void imageChanged();
    void zoomChanged(qreal zoom);
    void pointerMoved(const QPoint &point);

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        constexpr int checker = 12;
        for (int y = 0; y < height(); y += checker)
            for (int x = 0; x < width(); x += checker)
                painter.fillRect(x, y, checker, checker,
                                 ((x / checker) + (y / checker)) & 1 ? QColor(192, 192, 192) : Qt::white);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ < 1.0);
        painter.drawImage(QRectF(0, 0, image_.width() * zoom_, image_.height() * zoom_), image_);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            drawing_ = true;
            lastPoint_ = imagePoint(event->position());
            drawTo(lastPoint_);
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        const QPoint point = imagePoint(event->position());
        emit pointerMoved(point);
        if (drawing_) drawTo(point);
    }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) drawing_ = false;
    }
    void wheelEvent(QWheelEvent *event) override
    {
        if (event->modifiers() & Qt::ControlModifier) {
            setZoom(zoom_ * (event->angleDelta().y() > 0 ? 1.25 : 0.8));
            event->accept();
        } else {
            QWidget::wheelEvent(event);
        }
    }

private:
    QPoint imagePoint(const QPointF &point) const
    {
        return QPoint(int(std::floor(point.x() / zoom_)), int(std::floor(point.y() / zoom_)));
    }
    void drawTo(const QPoint &point)
    {
        QPainter painter(&image_);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setCompositionMode(tool_ == Tool::Eraser
                                       ? QPainter::CompositionMode_Clear
                                       : QPainter::CompositionMode_SourceOver);
        QPen pen(tool_ == Tool::Eraser ? Qt::transparent : color_);
        pen.setWidth(1);
        pen.setCapStyle(Qt::SquareCap);
        painter.setPen(pen);
        painter.drawLine(lastPoint_, point);
        lastPoint_ = point;
        update();
        emit imageChanged();
    }
    void updateSize()
    {
        resize(qMax(1, int(std::ceil(image_.width() * zoom_))),
               qMax(1, int(std::ceil(image_.height() * zoom_))));
        setMinimumSize(size());
    }

    QImage image_;
    QColor color_ = Qt::black;
    Tool tool_ = Tool::Pencil;
    qreal zoom_ = 1.0;
    bool drawing_ = false;
    QPoint lastPoint_;
};

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &initialPath = {})
    {
        setAcceptDrops(true);
        scroll_ = new QScrollArea;
        canvas_ = new Canvas;
        scroll_->setWidget(canvas_);
        scroll_->setAlignment(Qt::AlignCenter);
        scroll_->setBackgroundRole(QPalette::Dark);
        setCentralWidget(scroll_);
        createActions();
        connect(canvas_, &Canvas::imageChanged, this, [this] { setDirty(true); });
        connect(canvas_, &Canvas::zoomChanged, this, [this](qreal zoom) {
            zoomLabel_->setText(QStringLiteral("%1%").arg(qRound(zoom * 100)));
        });
        connect(canvas_, &Canvas::pointerMoved, this, [this](const QPoint &p) {
            positionLabel_->setText(QStringLiteral("%1, %2").arg(p.x()).arg(p.y()));
        });
        resize(1000, 720);
        if (!initialPath.isEmpty()) openPath(initialPath);
        updateTitle();
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        mayDiscard() ? event->accept() : event->ignore();
    }
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    }
    void dropEvent(QDropEvent *event) override
    {
        const auto urls = event->mimeData()->urls();
        if (!urls.isEmpty() && mayDiscard()) openPath(urls.first().toLocalFile());
    }

private:
    void createActions()
    {
        auto *file = menuBar()->addMenu(tr("&File"));
        auto *newAction = file->addAction(tr("&New"), QKeySequence::New, this, &MainWindow::newDocument);
        Q_UNUSED(newAction);
        file->addAction(tr("&Open…"), QKeySequence::Open, this, &MainWindow::openDocument);
        file->addAction(tr("&Save"), QKeySequence::Save, this, &MainWindow::saveDocument);
        file->addAction(tr("Save &As…"), QKeySequence::SaveAs, this, &MainWindow::saveDocumentAs);
        file->addSeparator();
        file->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);

        auto *view = menuBar()->addMenu(tr("&View"));
        view->addAction(tr("Zoom &In"), QKeySequence::ZoomIn, this, [this] { canvas_->setZoom(canvas_->zoom() * 1.25); });
        view->addAction(tr("Zoom &Out"), QKeySequence::ZoomOut, this, [this] { canvas_->setZoom(canvas_->zoom() * 0.8); });
        view->addAction(tr("&Actual Size"), QKeySequence(Qt::CTRL | Qt::Key_0), this, [this] { canvas_->setZoom(1.0); });

        auto *tools = addToolBar(tr("Tools"));
        tools->setMovable(false);
        auto *pencil = tools->addAction(tr("Pencil"));
        auto *eraser = tools->addAction(tr("Eraser"));
        pencil->setCheckable(true);
        eraser->setCheckable(true);
        pencil->setChecked(true);
        auto *group = new QActionGroup(this);
        group->setExclusive(true);
        group->addAction(pencil);
        group->addAction(eraser);
        connect(pencil, &QAction::triggered, this, [this] { canvas_->setTool(Canvas::Tool::Pencil); });
        connect(eraser, &QAction::triggered, this, [this] { canvas_->setTool(Canvas::Tool::Eraser); });
        tools->addSeparator();
        auto *color = tools->addAction(tr("Color…"));
        connect(color, &QAction::triggered, this, [this] {
            const QColor selected = QColorDialog::getColor(currentColor_, this, tr("Paint color"), QColorDialog::ShowAlphaChannel);
            if (selected.isValid()) {
                currentColor_ = selected;
                canvas_->setColor(selected);
            }
        });

        positionLabel_ = new QLabel(QStringLiteral("—, —"));
        zoomLabel_ = new QLabel(QStringLiteral("100%"));
        statusBar()->addPermanentWidget(positionLabel_);
        statusBar()->addPermanentWidget(zoomLabel_);
    }

    void newDocument()
    {
        if (!mayDiscard()) return;
        canvas_->setImage(newImage(kDefaultWidth, kDefaultHeight));
        path_.clear();
        setDirty(false);
    }
    void openDocument()
    {
        if (!mayDiscard()) return;
        const QString path = QFileDialog::getOpenFileName(this, tr("Open image"), {},
                                                          tr("Images (*.ff *.png *.jpg *.jpeg *.bmp *.tga);;All files (*)"));
        if (!path.isEmpty()) openPath(path);
    }
    void openPath(const QString &path)
    {
        QString error;
        QImage image;
        const bool isFarbfeld = QFileInfo(path).suffix().compare(QStringLiteral("ff"), Qt::CaseInsensitive) == 0;
        if (isFarbfeld)
            image = loadFarbfeld(path, &error);
        else {
            QImageReader reader(path);
            reader.setAutoTransform(true);
            image = reader.read();
            error = reader.errorString();
        }
        if (image.isNull()) {
            QMessageBox::critical(this, tr("Open failed"), tr("Could not open %1:\n%2").arg(path, error));
            return;
        }
        canvas_->setImage(image);
        // Conventional formats are imports. Never overwrite one with farbfeld
        // bytes when the user presses Save; route it through Save As instead.
        path_ = isFarbfeld ? path : QString();
        setDirty(!isFarbfeld);
        statusBar()->showMessage(tr("Opened %1 × %2 image").arg(image.width()).arg(image.height()), 3000);
    }
    bool saveDocument()
    {
        return path_.isEmpty() ? saveDocumentAs() : savePath(path_);
    }
    bool saveDocumentAs()
    {
        QString path = QFileDialog::getSaveFileName(this, tr("Save farbfeld image"), path_, tr("Farbfeld image (*.ff)"));
        if (path.isEmpty()) return false;
        if (!path.endsWith(QStringLiteral(".ff"), Qt::CaseInsensitive)) path += QStringLiteral(".ff");
        if (!savePath(path)) return false;
        path_ = path;
        updateTitle();
        return true;
    }
    bool savePath(const QString &path)
    {
        QString error;
        if (!saveFarbfeld(path, canvas_->image(), &error)) {
            QMessageBox::critical(this, tr("Save failed"), tr("Could not save %1:\n%2").arg(path, error));
            return false;
        }
        setDirty(false);
        statusBar()->showMessage(tr("Saved %1").arg(path), 3000);
        return true;
    }
    bool mayDiscard()
    {
        if (!dirty_) return true;
        const auto answer = QMessageBox::warning(this, tr("Unsaved changes"),
                                                  tr("Save changes before continuing?"),
                                                  QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                                  QMessageBox::Save);
        return answer == QMessageBox::Discard || (answer == QMessageBox::Save && saveDocument());
    }
    void setDirty(bool dirty)
    {
        dirty_ = dirty;
        updateTitle();
    }
    void updateTitle()
    {
        const QString name = path_.isEmpty() ? tr("Untitled") : QFileInfo(path_).fileName();
        setWindowTitle(QStringLiteral("%1%2 — wmdpaint Qt experiment").arg(name, dirty_ ? QStringLiteral("*") : QString()));
    }

    Canvas *canvas_ = nullptr;
    QScrollArea *scroll_ = nullptr;
    QLabel *positionLabel_ = nullptr;
    QLabel *zoomLabel_ = nullptr;
    QColor currentColor_ = Qt::black;
    QString path_;
    bool dirty_ = false;
};

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("wmdpaint-qt"));
    QApplication::setOrganizationName(QStringLiteral("wmdpaint"));
    MainWindow window(app.arguments().size() > 1 ? app.arguments().at(1) : QString());
    window.show();
    return app.exec();
}

#include "main.moc"
