#!/usr/bin/env python3
import asyncio
import json
import logging
import aiomqtt
from datetime import datetime
from aiogram import Bot, Dispatcher
from aiogram.filters import Command
from aiogram import Bot, Dispatcher, types

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

TOKEN = "----------------"
ADMIN_ID = 665730970
MQTT_HOST = "192.168.1.65"
MQTT_PORT = 1883

bot = Bot(token=TOKEN)
dp = Dispatcher()

status_message_id = None
last_sensor_data = {"temperature": None, "pressure": None}

def format_status():
    if last_sensor_data["temperature"] is None:
        return "Ожидание данных"
    return (
        f"Температура: {last_sensor_data['temperature']}C\n"
        f"Давление: {last_sensor_data['pressure']} гПа\n"
        f"Обновлено: {datetime.now().strftime('%H:%M:%S')}"
    )

async def update_status():
    global status_message_id
    while True:
        try:
            text = format_status()
            if status_message_id is None:
                msg = await bot.send_message(ADMIN_ID, text)
                status_message_id = msg.message_id
                logger.info("Создано сообщение со статусом")
            else:
                await bot.edit_message_text(
                    text, chat_id=ADMIN_ID, message_id=status_message_id
                )
                logger.info("Обновлено")
        except Exception as e:
            logger.error(f"Ошибка обновления статуса: {e}")
            status_message_id = None
        await asyncio.sleep(60)

async def mqtt_listener():
    while True:
        try:
            logger.info(f"Подключение к MQTT брокеру {MQTT_HOST}:{MQTT_PORT}")
            async with aiomqtt.Client(MQTT_HOST, MQTT_PORT) as client:
                logger.info("Подключено к MQTT брокеру")
                await client.subscribe("iot/#")
                async for message in client.messages:
                    topic = str(message.topic)
                    logger.info(f"Получено сообщение топик: {topic}")

                    if topic == "iot/sensors":
                        try:
                            data = json.loads(message.payload.decode())
                            last_sensor_data["temperature"] = data["temperature"]
                            last_sensor_data["pressure"] = data["pressure"]
                            logger.info(f"Данные датчиков: {data}")
                        except Exception as e:
                            logger.error(f"Ошибка парсинга: {e}")

                    elif topic == "iot/photo":
                        try:
                            caption = (
                                f"Обнаружено движение!\n"
                            )
                            await bot.send_photo(
                                ADMIN_ID,
                                types.BufferedInputFile(message.payload, filename="motion.jpg"),
                                caption=caption
                            )
                            logger.info("Фото отправлено")
                            await asyncio.sleep(5)
                            status_message_id = None
                        except Exception as e:
                            logger.error(f"Ошибка отправки фото: {e}")

        except Exception as e:
            logger.error(f"Ошибка MQTT: {e}, переподключение через 10 секунд...")
            await asyncio.sleep(10)

async def main():
    logger.info("Запуск бота...")
    await asyncio.gather(
        dp.start_polling(bot),
        mqtt_listener(),
        update_status()
    )

if __name__ == "__main__":
    asyncio.run(main())