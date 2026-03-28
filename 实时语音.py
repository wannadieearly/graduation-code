import speech_recognition as sr
import requests

# --- 配置区 ---
ARDUINO_IP = "192.168.97.33"  # 您的IP地址
PRINT_URL = f"http://{ARDUINO_IP}/print?text=床前明月光"
STOP_URL = f"http://{ARDUINO_IP}/stop"

# 定义你想要触发打印的关键词
TRIGGER_WORD = "床前明月光"


def recognize_speech_from_mic(recognizer, source):
    """从麦克风捕获5秒的固定长度音频并将其转换为文本。"""
    print(">>> 开始录音！")
    audio = recognizer.record(source, duration=5)
    print(">>> 5秒录音结束，正在识别...")
    try:
        text = recognizer.recognize_google(audio, language='zh-CN')
        print(f"识别到您说的是: {text}")
        return text
    except sr.UnknownValueError:
        print("无法识别音频，请说得更清晰一些。")
        return None
    except sr.RequestError as e:
        print(f"无法从Google Speech Recognition服务获取结果; {e}")
        return None


def send_command_to_arduino(url):
    """向 Arduino 发送 HTTP 请求"""
    try:
        print(f"正在向 {url} 发送指令...")
        response = requests.get(url, timeout=5)
        if response.status_code == 200:
            print("指令发送成功！Arduino 已响应。")
        else:
            print(f"发送失败，Arduino 返回状态码: {response.status_code}, 响应: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"网络错误: 无法连接到 Arduino ({ARDUINO_IP})。")
        print(f"错误详情: {e}")


def main():
    recognizer = sr.Recognizer()
    microphone = sr.Microphone()

    print(f"=== 语音盲文打印机 (关键词触发模式) ===")
    print(f"提示：说出 '{TRIGGER_WORD}' 开始打印，说 '停止' 终止。")

    while True:
        input("\n>>> 按回车键开始录音...")
        with microphone as source:
            command_text = recognize_speech_from_mic(recognizer, source)

        if command_text:
            # 1. 优先判断停止指令
            if "停止" in command_text:
                print(">>> 检测到停止指令，正在发送停止命令...")
                send_command_to_arduino(STOP_URL)

            # 2. 只有当识别到“床前明月光”时才打印
            elif TRIGGER_WORD in command_text:
                print(f">>> 识别到关键词 '{TRIGGER_WORD}'，立即执行打印...")
                send_command_to_arduino(PRINT_URL)

            # 3. 如果识别到了其他内容
            else:
                print(f">>> 识别内容为 '{command_text}'，不是指定指令，不执行操作。")


if __name__ == "__main__":
    main()