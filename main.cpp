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

// 箭头指示器类
class ArrowIndicator : public QWidget {
    Q_OBJECT
public:
    ArrowIndicator(QWidget *parent = nullptr) : QWidget(parent) {
        setFixedSize(20, 60);
        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Tool | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
    }

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
        arrow << QPointF(5, height()/2)
              << QPointF(15, height()/2 - 8)
              << QPointF(15, height()/2 + 8);
        painter.drawPolygon(arrow);
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            emit clicked();
        }
    }
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
        m_arrow = new ArrowIndicator();
        m_arrow->hide();
        connect(m_arrow, &ArrowIndicator::clicked, this, &RandomNumberApp::showFromSide);

        // 初始化隐藏位置
        m_screenWidth = QGuiApplication::primaryScreen()->availableGeometry().width();

        // 添加窗口阴影效果
        QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
        shadowEffect->setBlurRadius(15); // 增加阴影模糊半径
        shadowEffect->setColor(QColor(0, 0, 0, 85)); // 调整阴影透明度
        shadowEffect->setOffset(0, 2); // 轻微向下偏移
        this->setGraphicsEffect(shadowEffect);
    }

    ~RandomNumberApp() {
        // 保存配置
        saveConfig();
        delete m_arrow;
    }

    int hiddenOffset() const { return m_hiddenOffset; }
    void setHiddenOffset(int offset) {
        m_hiddenOffset = offset;
        move(m_screenWidth - width() + offset, y());
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
        if (m_arrow->isVisible()) {
            m_arrow->move(m_screenWidth - m_arrow->width(), y() + (height() - m_arrow->height())/2);
        }
        QWidget::moveEvent(event);
    }

    void hideEvent(QHideEvent *event) override {
        if (!isHiddenToSide) {
            m_arrow->hide();
        }
        QWidget::hideEvent(event);
    }

    // 重写paintEvent以确保圆角背景正确绘制
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 绘制圆角矩形背景
        p.setBrush(QBrush(QColor(250, 250, 250))); // 白色背景
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 15, 15); // 增加圆角半径到15

        // 绘制边框
        p.setPen(QPen(QColor(220, 220, 220), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 15, 15);
    }

    // 鼠标按下事件
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }

    // 鼠标移动事件
    void mouseMoveEvent(QMouseEvent *event) override {
        if (event->buttons() & Qt::LeftButton) {
            move(event->globalPosition().toPoint() - m_dragPosition);
            event->accept();
        }
    }

    // 鼠标释放事件
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            // 检查是否应该吸附到屏幕边缘
            QRect screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();
            QPoint currentPos = pos();

            // 如果靠近屏幕右侧边缘，则吸附到右侧
            if (currentPos.x() + width() > screenGeometry.width() - 20) {
                move(screenGeometry.width() - width(), currentPos.y());
            }
            // 如果靠近屏幕左侧边缘，则吸附到左侧
            else if (currentPos.x() < 20) {
                move(0, currentPos.y());
            }

            event->accept();
        }
    }

private slots:
    void generateRandomNumber() {
        std::uniform_int_distribution<int> dist(minValue, maxValue);
        label->setText(QString::number(dist(m_rng)));
    }

    void showFromSide() {
        showNormal();
        animateShow();
    }

private:
    void setupUI() {
        // 设置窗口属性
        setWindowTitle("随机数 (" + QString::number(minValue) + "-" + QString::number(maxValue) + ")");
        setFixedSize(300, 120); // 稍微增加尺寸以适应更大的圆角
        setWindowFlags(Qt::WindowMinimizeButtonHint | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground); // 启用透明背景

        // 设置样式表 - 调整控件样式以匹配圆角设计
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

        hideButton = new QPushButton("─", this); // 使用更细的减号
        hideButton->setObjectName("hideButton");
        hideButton->setGeometry(width() - 35, 10, 25, 25);

        connect(hideButton, &QPushButton::clicked, this, &RandomNumberApp::hideToSide);
        connect(button, &QPushButton::clicked, this, &RandomNumberApp::generateRandomNumber);
    }

    void hideToSide() {
        isHiddenToSide = true;
        this->hide();

        // 设置箭头位置并显示
        m_arrow->move(m_screenWidth - m_arrow->width(), y() + (height() - m_arrow->height())/2);
        m_arrow->show();
    }

    void animateShow() {
        isHiddenToSide = false;
        m_arrow->hide();
        this->showNormal();
    }

    QString getConfigPath() {
        // 获取应用程序所在目录
        QString appDir = QCoreApplication::applicationDirPath();
        return appDir + "/randomnumber_config.ini";
    }

    void loadConfig() {
        QString configFile = getConfigPath();

        // 如果配置文件不存在，则创建默认配置
        if (!QFile::exists(configFile)) {
            createDefaultConfig();
        }

        QSettings settings(configFile, QSettings::IniFormat);

        // 读取配置值，如果没有则使用默认值
        minValue = settings.value("Range/min", 1).toInt();
        maxValue = settings.value("Range/max", 47).toInt();

        // 验证范围有效性
        if (minValue >= maxValue) {
            minValue = 1;
            maxValue = 47;
            // 保存修正后的值
            saveConfig();
        }
    }

    void createDefaultConfig() {
        QString configFile = getConfigPath();
        QSettings settings(configFile, QSettings::IniFormat);

        // 设置默认值
        settings.setValue("Range/min", 1);
        settings.setValue("Range/max", 47);

        // 立即同步到文件
        settings.sync();
    }

    void saveConfig() {
        QString configFile = getConfigPath();
        QSettings settings(configFile, QSettings::IniFormat);

        settings.setValue("Range/min", minValue);
        settings.setValue("Range/max", maxValue);

        // 确保立即写入文件
        settings.sync();
    }

    QPushButton *button;
    QPushButton *hideButton;
    QLabel *label;
    ArrowIndicator *m_arrow;
    std::mt19937 m_rng;
    int m_screenWidth;
    int m_hiddenOffset;
    bool isHiddenToSide = false;
    QPoint m_dragPosition;
    int minValue = 1;
    int maxValue = 47;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    RandomNumberApp window;
    window.show();
    return app.exec();
}

#include "main.moc"