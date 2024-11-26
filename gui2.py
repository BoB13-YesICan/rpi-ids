import os
import subprocess
import threading
from PyQt5.QtWidgets import QApplication, QVBoxLayout, QHBoxLayout, QTextEdit, QLabel, QPushButton, QWidget, QGridLayout, QGroupBox
from PyQt5.QtCore import QTimer, pyqtSignal

# Qt 환경 설정
os.environ["QT_QPA_PLATFORM"] = "xcb" #

class CANMonitorApp(QWidget):
    stdout_signal = pyqtSignal(str)  # 문자열 데이터를 전달하는 신호

    def __init__(self, c_program_path, log_filename):
        super().__init__()
        self.stdout_signal.connect(self.update_stdout)  # 신호를 슬롯에 연결
        self.c_program_path = c_program_path  # C 프로그램 실행 파일 경로
        self.log_filename = log_filename      # 로그 파일 이름
        self.attack_counts = {  # 공격별 카운트를 저장하는 딕셔너리
            "DoS": 0,
            "Replay": 0,
            "Fuzzing": 0,
            "Suspension": 0,
            "Masquerade": 0
        }
        # 각 공격 ID 저장
        self.attack_ids = {k: "" for k in self.attack_counts.keys()}
        self.total_attack_counts = 0
        self.initUI()
        self.c_process = None

    def initUI(self):
        self.setWindowTitle("CAN Monitor GUI")
        self.setGeometry(200, 200, 800, 600)

        # 메인 레이아웃
        main_layout = QVBoxLayout()

        # 상단 섹션 (Title)
        title_label = QLabel("실시간 탐지!!! WARNING!")
        title_label.setStyleSheet("font-size: 20px; font-weight: bold; text-align: center;")
        main_layout.addWidget(title_label)

        # 중단 섹션 (공격 카운트와 ID 표시)
        middle_layout = QGridLayout()

        # 공격별 카운트와 ID
        self.attack_labels = {}
        # attack_types = ["공격 패킷 갯수", "DoS [ID]", "Replay [ID]", "Fuzzing [ID]", "Suspension [ID]", "Masquerade [ID]"]
        for i, attack in enumerate(self.attack_counts.keys()):
            label = QLabel(f"{attack}: 0")
            label.setStyleSheet("background-color: lightgreen; border: 1px solid black; padding: 5px;")
            middle_layout.addWidget(label, i // 2, i % 2)
            self.attack_labels[attack] = label

        middle_widget = QWidget()
        middle_widget.setLayout(middle_layout)
        main_layout.addWidget(middle_widget)

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

        button_widget = QWidget()
        button_widget.setLayout(button_layout)
        main_layout.addWidget(button_widget)

        # 메인 레이아웃 설정
        self.setLayout(main_layout)

        # 로그 파일 갱신 타이머
        self.log_timer = QTimer(self)
        self.log_timer.timeout.connect(self.update_log)

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
        for attack in self.attack_counts.keys():
            if f"[{attack}]" in line:  # 출력에 공격 유형이 포함된 경우
                self.attack_counts[attack] += 1
                self.update_attack_labels(attack)
                break
        if "Malicious packet" in line:
            self.total_attack_counts+=1

    # def update_attack_labels(self, attack):
    #     """
    #     특정 공격 유형의 카운트를 업데이트하여 레이블에 반영
    #     """
    #     self.attack_labels[attack].setText(f"{attack}: {self.attack_counts[attack]}")
    def update_attack_labels(self, attack):
        """
        특정 공격 유형의 카운트를 업데이트하여 레이블에 반영하고,
        배경색을 0.5초 동안 빨간색으로 변경.
        """
        # 카운트 업데이트
        self.attack_labels[attack].setText(f"{attack}: {self.attack_counts[attack]}")
        
        # 기존 배경색 저장
        original_style = "background-color: lightgreen; border: 1px solid black; padding: 5px;"
        # 빨간색으로 변경
        self.attack_labels[attack].setStyleSheet("background-color: red; border: 1px solid black; padding: 5px;")

        # 0.5초 후 원래 색상으로 복원
        QTimer.singleShot(500, lambda: self.attack_labels[attack].setStyleSheet(original_style))

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
