import os
import subprocess
import threading
import re
from PyQt5.QtWidgets import QApplication, QVBoxLayout, QHBoxLayout, QTextEdit, QLabel, QPushButton, QWidget, QGridLayout, QDialog,  QScrollArea, QMainWindow
from PyQt5.QtCore import QTimer, pyqtSignal, Qt, QMargins
from PyQt5.QtChart import QChart, QChartView, QPieSeries
from PyQt5.QtGui import QPainter


# Qt 환경 설정
os.environ["QT_QPA_PLATFORM"] = "xcb" #

class CANMonitorApp(QWidget):
    stdout_signal = pyqtSignal(str)  # 문자열 데이터를 전달하는 신호

    def __init__(self, c_program_path, log_filename):
        super().__init__()
        self.stdout_signal.connect(self.update_stdout)  # 신호를 슬롯에 연결
        self.c_program_path = c_program_path  # C 프로그램 실행 파일 경로
        self.log_filename = log_filename      # 로그 파일 이름
        self.attack_id_counts = {  # 공격별 카운트를 저장하는 딕셔너리
            "DoS": {},
            "Replay": {},
            "Fuzzing": {},
            "Suspension": {},
            "Masquerade": {},
        }

        self.attack_counts = {  # 공격별 카운트를 저장하는 딕셔너리
            "DoS": 0,
            "Replay": 0,
            "Fuzzing": 0,
            "Suspension": 0,
            "Masquerade": 0,
            "All":0
        }

        # 각 공격 ID 저장
        self.attack_ids = {k: "" for k in self.attack_id_counts.keys()}
        self.total_counts=0
        self.total_attack_counts = 0
        self.initUI()
        self.c_process = None

    def initUI(self):
        self.setWindowTitle("CAN Monitor GUI")
        self.setGeometry(200, 200, 800, 600)

        # 메인 레이아웃
        main_layout = QVBoxLayout()

        # 상단 섹션 (Title)
        self.title_label = QLabel("NORMAL")
        self.title_label.setStyleSheet("font-size: 20px; font-weight: bold; text-align: center;")
        main_layout.addWidget(self.title_label)

        # 중단 섹션 (공격 카운트와 ID 표시)
        middle_layout = QGridLayout()

        # 공격별 카운트와 ID
        self.attack_labels = {}
        for i, attack in enumerate(self.attack_counts.keys()):
            label = QLabel(f"{attack}: 0")
            label.setStyleSheet("background-color: lightgreen; border: 1px solid black; padding: 5px;")
            middle_layout.addWidget(label, i // 2, i % 2)
            self.attack_labels[attack] = label
        self.attack_labels["All"].setStyleSheet("background-color: skyblue; border: 1px solid black; padding: 5px;")
        middle_widget = QWidget()
        middle_widget.setLayout(middle_layout)
        main_layout.addWidget(middle_widget)

        # 원형 그래프 추가
        self.chart_view = self.create_pie_chart()
        main_layout.addWidget(self.chart_view)

        # 중앙 섹션 (로그 출력 영역)
        log_layout = QHBoxLayout()

        # C 프로그램 출력 영역
        self.stdout_text = QTextEdit(self)
        self.stdout_text.setReadOnly(True)
        self.stdout_text.setPlaceholderText("기존 코드에서 C 프로그램 출력하는 곳")
        log_layout.addWidget(self.stdout_text)

        # 로그 파일 출력 영역
        self.log_text = QTextEdit(self)
        self.log_text.setReadOnly(True)
        self.log_text.setPlaceholderText("기존 코드에서 로그 파일 출력하는 곳")
        log_layout.addWidget(self.log_text)

        log_widget = QWidget()
        log_widget.setLayout(log_layout)
        main_layout.addWidget(log_widget)

        # 하단 섹션 (버튼)
        button_layout = QHBoxLayout()

        # "Start" 버튼
        self.start_button = QPushButton("start")
        self.start_button.clicked.connect(self.start_monitoring)
        button_layout.addWidget(self.start_button)

        self.stop_button = QPushButton("stop")
        self.stop_button.clicked.connect(self.stop_monitoring)
        button_layout.addWidget(self.stop_button)

        self.graph_button=QPushButton("Graph")
        self.graph_button.clicked.connect(self.show_detection_detail)
        button_layout.addWidget(self.graph_button)

        button_widget = QWidget()
        button_widget.setLayout(button_layout)
        main_layout.addWidget(button_widget)

        # 메인 레이아웃 설정
        self.setLayout(main_layout)

        # 로그 파일 갱신 타이머
        self.log_timer = QTimer(self)
        self.log_timer.timeout.connect(self.update_log)

    def create_pie_chart(self):
        """
        Create and return a QChartView containing a pie chart.
        """
        # Create a QPieSeries
        series = QPieSeries()
        for attack, count in self.attack_counts.items():
            if attack != "All":
                series.append(f"{attack}: {count}", count)

        # Create a chart and set its title
        chart = QChart()
        chart.addSeries(series)
        chart.setTitle("Attack Count Distribution")
        chart.setMargins(QMargins(10, 10, 10, 10))  # 여백 설정
        chart.setMinimumSize(600, 400)  # 그래프 크기 설정

        chart.legend().setVisible(True)
        chart.legend().setAlignment(Qt.AlignRight)
        # chart.legend().setOrientation(Qt.Vertical)  # 세로 방향으로 나열

        # Create a QChartView to display the chart
        chart_view = QChartView(chart)
        chart_view.setRenderHint(QPainter.Antialiasing)
        chart_view.setMinimumSize(700, 500)  # 차트 뷰 크기 설정

        return chart_view
    
    def update_pie_chart(self):
        """
        Update the pie chart based on the current attack counts.
        """
        # Get the current chart from the chart view
        chart = self.chart_view.chart()

        # Clear the old series
        chart.removeAllSeries()

        # Create a new QPieSeries
        series = QPieSeries()
        for attack, count in self.attack_counts.items():
            if attack != "All":
                series.append(f"{attack}: {count}", count)

        # Add the new series to the chart
        chart.addSeries(series)
        # Ensure legend remains visible
        chart.legend().setVisible(True)
        chart.legend().setAlignment(Qt.AlignRight)  # 범례를 오른쪽에 정렬

    def show_detection_detail(self):
        # 새 창 생성
        dialog = QDialog(self)
        dialog.setWindowTitle("Detection Details")
        dialog.setGeometry(300, 300, 800, 600)

        # 메인 레이아웃 설정
        main_layout = QVBoxLayout()

        # 공격별 상세 정보 표시
        for attack_type, attack_data in self.attack_id_counts.items():
            if not attack_data:  # 데이터가 없는 경우 넘어감
                continue

            # 제목 표시
            title_label = QLabel(f"Attack Type: {attack_type}")
            title_label.setStyleSheet("font-size: 16px; font-weight: bold; margin: 10px 0;")
            main_layout.addWidget(title_label)

            # 원형 그래프 생성 및 추가
            chart_view = self.create_attack_pie_chart(attack_type, attack_data)
            main_layout.addWidget(chart_view)

        # 스크롤 영역 설정 (그래프가 많아질 경우 대비)
        scroll_area = QScrollArea()
        scroll_widget = QWidget()
        scroll_widget.setLayout(main_layout)
        scroll_area.setWidget(scroll_widget)
        scroll_area.setWidgetResizable(True)

        # 다이얼로그 레이아웃 설정
        dialog_layout = QVBoxLayout()
        dialog_layout.addWidget(scroll_area)
        dialog.setLayout(dialog_layout)

        # 다이얼로그 표시
        dialog.exec_()

    def create_attack_pie_chart(self, attack_type, attack_data):
        """
        공격 유형별 데이터를 기반으로 원형 그래프 생성
        """
        series = QPieSeries()
        for attack_id, count in attack_data.items():
            series.append(f"{attack_id}: {count}", count)

        # 차트 설정
        chart = QChart()
        chart.addSeries(series)
        chart.setTitle(f"{attack_type} Attack Details")
        chart.setMargins(QMargins(10, 10, 10, 10))
        chart.legend().setVisible(True)
        chart.legend().setAlignment(Qt.AlignRight)

        # 차트 뷰 생성
        chart_view = QChartView(chart)
        chart_view.setRenderHint(QPainter.Antialiasing)
        chart_view.setMinimumSize(600, 400)

        return chart_view
    # 나머지 함수는 기존 코드와 동일
    def start_monitoring(self):
        if not os.path.exists(self.c_program_path):#C 프로그램 파일이 존재하는지 확인.
            self.stdout_text.append(f"Error: {self.c_program_path} not found!")
            self.scroll_to_bottom(self.stdout_text)
            return

        try:
            self.c_process = subprocess.Popen(#C 프로그램을 실행하고 stdout과 stderr를 읽음.
                [self.c_program_path, self.log_filename],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            self.stdout_thread = threading.Thread(target=self.read_stdout)#별도 스레드에서 stdout을 읽는 read_stdout 실행.
            self.stdout_thread.daemon = True
            self.stdout_thread.start()
            # self.log_timer.start(100)  # 1초(1000ms) 간격으로 로그 업데이트.
            # QTimer.singleShot(0, self.log_timer.start(100))  # 메인 스레드에서 타이머 시작
            QTimer.singleShot(0, lambda: self.log_timer.start(1000))  # 1초 간격
        except Exception as e:
            self.stdout_text.append(f"Error starting C program: {e}")
            self.scroll_to_bottom(self.stdout_text)

    def stop_monitoring(self):
        if self.c_process:
            self.c_process.terminate()  # C 프로그램 종료
            self.c_process = None
        self.log_timer.stop()  # 로그 파일 갱신 중지

    def read_stdout(self):
        # C 프로그램 stdout을 읽어 GUI에 출력
        while self.c_process and self.c_process.poll() is None:
            line = self.c_process.stdout.readline()
            if line:
                self.stdout_signal.emit(line.strip())  # 메인 스레드로 전달

    def update_stdout(self, text):
        # 메인 스레드에서 GUI 업데이트
        self.stdout_text.append(text)
        self.scroll_to_bottom(self.stdout_text)
        self.parse_attack(text)  # 공격 분석

    def parse_attack(self, line):
        
        """
        표준 출력에서 공격 유형을 파싱하고, 카운트를 업데이트
        """
        attack_match = re.search(r"\[(\w+)\] \[(\d+)\] \[(\w+)\] (.+)", line)

        if attack_match:
            attack_type = attack_match.group(1)  # 예: DoS
            attack_id = attack_match.group(2)   # 예: 000
            if attack_type not in self.attack_counts:
                print(attack_type+"is not in self.attack_counts")
                return 
            if attack_type not in self.attack_id_counts:
                print("Not attack_type "+ attack_type)
                return
            if attack_id not in self.attack_id_counts[attack_type]:
                print("New Detection ID "+attack_id)
                self.attack_id_counts[attack_type][attack_id] = 0
            self.attack_id_counts[attack_type][attack_id] += 1  # count가 없으면 증가
            self.attack_counts[attack_type]+=1
            self.attack_counts["All"]+=1
            self.total_attack_counts+=1
            
            self.update_attack_labels(attack_type)
        else:print("Not parse attack info")
    

        # for attack in self.attack_counts.keys():
        #     if f"[{attack}]" in line:  # 출력에 공격 유형이 포함된 경우
        #         self.attack_counts[attack] += 1
        #         self.attack_counts["All"] += 1
        #         self.update_attack_labels(attack)
        #         break

        if "Malicious packet" in line:
            self.total_attack_counts+=1

    def update_attack_labels(self, attack):
        # 카운트 업데이트
        self.attack_labels[attack].setText(f"{attack}: {self.attack_counts[attack]}")
        self.attack_labels["All"].setText(f"{"All"}: {self.attack_counts["All"]}")
        # print("label update "+attack)
        # 기존 배경색 저장
        original_title_style="font-size: 20px; font-weight: bold; text-align: center;"
        original_title="NORMAL"
        original_style = "background-color: lightgreen; border: 1px solid black; padding: 5px;"
        # 빨간색으로 변경
        self.attack_labels[attack].setStyleSheet("background-color: red; border: 1px solid black; padding: 5px;")
        self.title_label.setStyleSheet("color: red; font-size: 20px; font-weight: bold; text-align: center;")
        self.title_label.setText("WARNING!!!")

        # 0.5초 후 스타일 복원
        def restore_styles():
            self.attack_labels[attack].setStyleSheet(original_style)
            self.title_label.setStyleSheet(original_title_style)
            self.title_label.setText(original_title)
        # 0.5초 후 원래 색상으로 복원
        QTimer.singleShot(500, restore_styles)
        # Update the pie chart
        self.update_pie_chart()

    def update_log(self):
        # 로그 파일 내용을 읽어와 GUI에 표시
        try:
            with open(self.log_filename, "r") as file:
                self.log_text.setPlainText(file.read())
                self.scroll_to_bottom(self.log_text)  # 로그 파일 갱신 시 스크롤 아래로 이동
        except FileNotFoundError:
            self.log_text.setPlainText("Log file not found!")
            self.scroll_to_bottom(self.log_text)

    def scroll_to_bottom(self, text_edit):
        """
        QTextEdit의 스크롤을 맨 아래로 이동
        """
        cursor = text_edit.textCursor()
        cursor.movePosition(cursor.End)
        text_edit.setTextCursor(cursor)
        text_edit.ensureCursorVisible()

    def closeEvent(self, event):
        self.stop_monitoring()
        event.accept()

if __name__ == "__main__":
    c_program_path = "./scripts/ids"
    log_filename = "temp.log"

    app = QApplication([])
    window = CANMonitorApp(c_program_path, log_filename)
    window.show()
    app.exec_()