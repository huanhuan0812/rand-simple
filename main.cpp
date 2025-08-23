#include <QApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <random>
#include <QPainter>
#include <QStyleOption>
#include <QEvent>
#include <QCloseEvent>
#include <QGraphicsDropShadowEffect>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QScreen>

// 箭头指示器类
class ArrowIndicator : public QWidget {
    Q_OBJECT
public:
    enum ArrowDirection {
        Left,
        Right
    };

    ArrowIndicator(ArrowDirection direction, QWidget *parent = nullptr) 
        : QWidget(parent), m_direction(direction) {
        setFixedSize(20, 60);
        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Tool | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    ArrowDirection direction() const { return m_direction; }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(100, 100, 100, 180));
        painter.setPen(Qt::NoPen);

        // 绘制圆角矩形背景
        painter.drawRoundedRect(rect(), 3, 3);

        // 绘制箭头
        painter.setBrush(Qt::white);
        QPolygonF arrow;
        
        if (m_direction == Right) {
            // 向右的箭头
            arrow << QPointF(5, height()/2)
                  << QPointF(15, height()/2 - 8)
                  << QPointF(15, height()/2 + 8);
        } else {
            // 向左的箭头
            arrow << QPointF(15, height()/2)
                  << QPointF(5, height()/2 - 8)
                  << QPointF(5, height()/2 + 8);
        }
        painter.drawPolygon(arrow);
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            // 记录拖动起始位置
            m_dragStartPosition = event->globalPosition().toPoint();
            m_isDragging = false;
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (event->buttons() & Qt::LeftButton) {
            // 计算移动距离
            QPoint delta = event->globalPosition().toPoint() - m_dragStartPosition;
            
            // 如果移动距离超过一定阈值，则开始拖动
            if (!m_isDragging && delta.manhattanLength() > QApplication::startDragDistance()) {
                m_isDragging = true;
            }
            
            if (m_isDragging) {
                // 移动窗口
                move(pos() + delta);
                m_dragStartPosition = event->globalPosition().toPoint();
                event->accept();
            }
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            if (!m_isDragging) {
                // 如果没有拖动，则发送点击信号
                emit clicked();
            } else {
                // 拖动结束后，确保箭头停留在屏幕侧面
                ensureOnScreenEdge();
            }
            m_isDragging = false;
            event->accept();
        }
    }

private:
    void ensureOnScreenEdge() {
        QScreen *screen = QGuiApplication::primaryScreen();
        QRect screenGeometry = screen->availableGeometry();
        QPoint currentPos = pos();
        
        int midY = currentPos.y() + height() / 2;
        
        // 判断距离哪边更近
        int distanceToLeft = currentPos.x();
        int distanceToRight = screenGeometry.width() - (currentPos.x() + width());
        
        if (distanceToLeft < distanceToRight) {
            // 靠近左侧，吸附到左侧
            move(0, qMax(0, qMin(midY - height()/2, screenGeometry.height() - height())));
            m_direction = Left;
        } else {
            // 靠近右侧，吸附到右侧
            move(screenGeometry.width() - width(), qMax(0, qMin(midY - height()/2, screenGeometry.height() - height())));
            m_direction = Right;
        }
        
        update(); // 重绘箭头方向
    }

private:
    QPoint m_dragStartPosition;
    bool m_isDragging = false;
    ArrowDirection m_direction;
};

class RandomNumberApp : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int hiddenOffset READ hiddenOffset WRITE setHiddenOffset)

public:
    RandomNumberApp(QWidget *parent = nullptr) : QWidget(parent), m_hiddenOffset(0) {
        m_rng.seed(QTime::currentTime().msec());

        // 加载配置
        loadConfig();

        setupUI();

        // 初始化箭头指示器
        m_rightArrow = new ArrowIndicator(ArrowIndicator::Right);
        m_rightArrow->hide();
        connect(m_rightArrow, &ArrowIndicator::clicked, this, &RandomNumberApp::showFromRight);
        
        m_leftArrow = new ArrowIndicator(ArrowIndicator::Left);
        m_leftArrow->hide();
        connect(m_leftArrow, &ArrowIndicator::clicked, this, &RandomNumberApp::showFromLeft);

        // 初始化屏幕尺寸
        m_screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();

        // 添加窗口阴影效果
        QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
        shadowEffect->setBlurRadius(15);
        shadowEffect->setColor(QColor(0, 0, 0, 85));
        shadowEffect->setOffset(0, 2);
        this->setGraphicsEffect(shadowEffect);
    }

    ~RandomNumberApp() {
        // 保存配置
        saveConfig();
        delete m_rightArrow;
        delete m_leftArrow;
    }

    int hiddenOffset() const { return m_hiddenOffset; }
    void setHiddenOffset(int offset) {
        m_hiddenOffset = offset;
        move(m_screenGeometry.width() - width() + offset, y());
    }

protected:
    void changeEvent(QEvent *event) override {
        if (event->type() == QEvent::WindowStateChange) {
            if (isMinimized()) {
                hideToSide();
                event->ignore();
            }
        }
        QWidget::changeEvent(event);
    }

    void moveEvent(QMoveEvent *event) override {
        // 更新箭头位置
        updateArrowPositions();
        QWidget::moveEvent(event);
    }

    void resizeEvent(QResizeEvent *event) override {
        updateArrowPositions();
        QWidget::resizeEvent(event);
    }

    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        m_isAutoHidden = false;
    }

    void hideEvent(QHideEvent *event) override {
        if (!isHiddenToSide && !m_isAutoHidden) {
            m_rightArrow->hide();
            m_leftArrow->hide();
        }
        QWidget::hideEvent(event);
    }

    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 绘制圆角矩形背景
        p.setBrush(QBrush(QColor(250, 250, 250)));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 15, 15);

        // 绘制边框
        p.setPen(QPen(QColor(220, 220, 220), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 15, 15);
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            m_isDragging = true;
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (event->buttons() & Qt::LeftButton && m_isDragging) {
            move(event->globalPosition().toPoint() - m_dragPosition);
            event->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            m_isDragging = false;
            
            // 在鼠标释放时检测是否应该自动隐藏
            checkAutoHideOnRelease();
            
            event->accept();
        }
    }

private slots:
    void generateRandomNumber() {
        std::uniform_int_distribution<int> dist(minValue, maxValue);
        label->setText(QString::number(dist(m_rng)));
    }

    void showFromRight() {
        showNormal();
        animateShow(Right);
    }

    void showFromLeft() {
        showNormal();
        animateShow(Left);
    }

private:
    enum HideSide { Left, Right };

    void setupUI() {
        // 设置窗口属性
        setWindowTitle("随机数 (" + QString::number(minValue) + "-" + QString::number(maxValue) + ")");
        setFixedSize(300, 120);
        setWindowFlags(Qt::WindowMinimizeButtonHint | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);

        // 设置样式表
        setStyleSheet(R"(
            QPushButton {
                font-size: 16px;
                background-color: #4CAF50;
                color: white;
                border-radius: 30px;
                border: none;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
            QPushButton:pressed {
                background-color: #3d8b40;
            }
            QPushButton#hideButton {
                font-size: 18px;
                border-radius: 6px;
                background-color: #f0f0f0;
                color: #666;
                border: 1px solid #ddd;
            }
            QPushButton#hideButton:hover {
                background-color: #e0e0e0;
            }
            QLabel {
                font-size: 28px;
                font-weight: bold;
                border-radius: 12px;
                background-color: #f8f8f8;
                border: 2px solid #e0e0e0;
                color: #333;
            }
        )");

        button = new QPushButton("生成", this);
        button->setGeometry(width() - 100, 30, 60, 60);

        label = new QLabel(" ", this);
        label->setGeometry(25, 30, 150, 60);
        label->setAlignment(Qt::AlignCenter);

        hideButton = new QPushButton("─", this);
        hideButton->setObjectName("hideButton");
        hideButton->setGeometry(width() - 35, 10, 25, 25);

        connect(hideButton, &QPushButton::clicked, this, &RandomNumberApp::hideToSide);
        connect(button, &QPushButton::clicked, this, &RandomNumberApp::generateRandomNumber);
    }

    void hideToSide() {
        isHiddenToSide = true;
        m_isAutoHidden = false;
        this->hide();

        // 根据最后吸附的边显示对应的箭头
        if (m_lastHideSide == Right) {
            m_rightArrow->show();
            m_leftArrow->hide();
        } else {
            m_leftArrow->show();
            m_rightArrow->hide();
        }
        
        updateArrowPositions();
    }

    void checkAutoHideOnRelease() {
        if (isVisible() && !isHiddenToSide && !m_isAutoHidden) {
            // 检查是否超过一半在屏幕外
            if (isMostlyOffScreen()) {
                autoHideToSide();
            } else {
                // 正常吸附逻辑
                QPoint currentPos = pos();
                if (currentPos.x() + width() > m_screenGeometry.width() - 20) {
                    move(m_screenGeometry.width() - width(), currentPos.y());
                    m_lastHideSide = Right;
                } else if (currentPos.x() < 20) {
                    move(0, currentPos.y());
                    m_lastHideSide = Left;
                }
            }
        }
    }

    void autoHideToSide() {
        m_isAutoHidden = true;
        isHiddenToSide = true;
        this->hide();

        // 判断应该隐藏到哪一侧
        QPoint currentPos = pos();
        
        if (currentPos.x() + width() / 2 < m_screenGeometry.width() / 2) {
            // 主要部分在左侧，隐藏到左侧
            m_lastHideSide = Left;
            m_leftArrow->show();
            m_rightArrow->hide();
        } else {
            // 主要部分在右侧，隐藏到右侧
            m_lastHideSide = Right;
            m_rightArrow->show();
            m_leftArrow->hide();
        }
        
        updateArrowPositions();
    }

    void animateShow(HideSide side) {
        isHiddenToSide = false;
        m_isAutoHidden = false;
        m_rightArrow->hide();
        m_leftArrow->hide();
        this->showNormal();
        
        // 根据显示的方向调整窗口位置
        if (side == Right) {
            move(m_screenGeometry.width() - width(), y());
        } else {
            move(0, y());
        }
    }

    void updateArrowPositions() {
        if (m_rightArrow->isVisible()) {
            m_rightArrow->move(m_screenGeometry.width() - m_rightArrow->width(), 
                              y() + (height() - m_rightArrow->height())/2);
        }
        if (m_leftArrow->isVisible()) {
            m_leftArrow->move(0, y() + (height() - m_leftArrow->height())/2);
        }
    }

    bool isMostlyOffScreen() const {
        QRect visibleRect = getVisibleScreenRect();
        QRect windowRect(pos(), size());
        QRect intersection = visibleRect.intersected(windowRect);
        return intersection.width() <= width() / 2;
    }

    QRect getVisibleScreenRect() const {
        // 获取所有屏幕的几何区域
        QList<QScreen*> screens = QGuiApplication::screens();
        QRect combinedRect;
        
        for (QScreen* screen : screens) {
            combinedRect = combinedRect.united(screen->availableGeometry());
        }
        
        return combinedRect;
    }

    QString getConfigPath() {
        QString appDir = QCoreApplication::applicationDirPath();
        return appDir + "/randomnumber_config.ini";
    }

    void loadConfig() {
        QString configFile = getConfigPath();

        if (!QFile::exists(configFile)) {
            createDefaultConfig();
        }

        QSettings settings(configFile, QSettings::IniFormat);

        minValue = settings.value("Range/min", 1).toInt();
        maxValue = settings.value("Range/max", 47).toInt();

        if (minValue >= maxValue) {
            minValue = 1;
            maxValue = 47;
            saveConfig();
        }
    }

    void createDefaultConfig() {
        QString configFile = getConfigPath();
        QSettings settings(configFile, QSettings::IniFormat);

        settings.setValue("Range/min", 1);
        settings.setValue("Range/max", 47);
        settings.sync();
    }

    void saveConfig() {
        QString configFile = getConfigPath();
        QSettings settings(configFile, QSettings::IniFormat);

        settings.setValue("Range/min", minValue);
        settings.setValue("Range/max", maxValue);
        settings.sync();
    }

    QPushButton *button;
    QPushButton *hideButton;
    QLabel *label;
    ArrowIndicator *m_rightArrow;
    ArrowIndicator *m_leftArrow;
    std::mt19937 m_rng;
    QRect m_screenGeometry;
    int m_hiddenOffset;
    bool isHiddenToSide = false;
    bool m_isAutoHidden = false;
    bool m_isDragging = false;
    QPoint m_dragPosition;
    int minValue = 1;
    int maxValue = 47;
    HideSide m_lastHideSide = Right;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    RandomNumberApp window;
    window.show();
    return app.exec();
}

#include "main.moc"