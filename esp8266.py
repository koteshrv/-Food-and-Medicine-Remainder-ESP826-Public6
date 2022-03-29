import pyrebase, json, threading, telegram, os, time
from telegram.ext.updater import Updater
from telegram.update import Update
from telegram.ext.callbackcontext import CallbackContext
from telegram.ext.commandhandler import CommandHandler
from telegram.ext.messagehandler import MessageHandler
from telegram.ext.filters import Filters
from datetime import datetime, timedelta
from zoneinfo import ZoneInfo
from dotenv import load_dotenv

load_dotenv()

apiKey = os.getenv('apiKey')
authDomain = os.getenv('authDomain')
databaseURL = os.getenv('databaseURL')
storageBucket = os.getenv('storageBucket')
TOKEN = os.getenv('TOKEN')
userId = os.getenv('userId')

updater = Updater(TOKEN, use_context=True)
global pirMotionCount
pirMotionCount = 0

firebaseConfig = {
    "apiKey": apiKey,
    "authDomain": authDomain,
    "databaseURL": databaseURL,
    "storageBucket": storageBucket,
};

print('Started telegram bot successfully at ' + str(datetime.now()))
telegramBot = telegram.Bot(token = TOKEN)

def indexKeyHelp(update: Update, context: CallbackContext):
  update.message.reply_text("""currentDate keys :-
 * 0 morningStatus
 * 1 afternoonStatus
 * 2 nightStatus
 * 3 morningTelegramAlert
 * 4 afternoonTelegramAlert
 * 5 nightTelegramAlert
 * 6 morningStatusT
 * 7 afternoonStatusT
 * 8 nightStatusT
 *
 * timings keys
 * 0 morningTabletAlertH
 * 1 morningTabletAlertM
 * 2 afternoonTabletAlertH
 * 3 afternoonTabletAlertM
 * 4 nightTabletAlertH
 * 5 nightTabletAlertM
 * 6 morningTiffinAlertH
 * 7 morningTiffinAlertM
 * 8 afternoonLunchAlertH
 * 9 afternoonLunchAlertM
 * 10 nightDinnerAlertH
 * 11 nightDinnerAlertM""")

def sendToTelegram(message):
	telegramBot.send_message(chat_id = userId, text = message)

def start(update: Update, context: CallbackContext):
	update.message.reply_text(
		"Hello , Welcome to the Food and Medicine Remainder using ESP8266 Telegram Bot. Please write\
		/help to see the commands available.")

def help(update: Update, context: CallbackContext):
	update.message.reply_text("""Available Commands :-
	/showLog - shows log data of that particular day
	/update - updates timings with key and value provided
	/index - shows the index assigned for currentdate and timings""")

def unknown_text(update: Update, context: CallbackContext):
    update.message.reply_text(
        "Sorry I can't recognize you , you said '%s'" % update.message.text)


def showLog(update: Update, context: CallbackContext) :
  date = ' '.join(update.message.text.split(' ',1)[1:])
  if date != None and date != ' ':
    log = db.child(date).get().val()
    if(log != None):
      update.message.reply_text(json.dumps(log, indent=4))
    else:
      update.message.reply_text("Invalid date entered or entered date not registered in firebase, Try again with format '7 December 2021'")
  else:
    update.message.reply_text("No date entered, Try again with format '7 December 2021'")

def pirLog(update: Update, context: CallbackContext) :
  date = ' '.join(update.message.text.split(' ',1)[1:])
  if date != None and date != ' ':
    log = db.child("pirStatus").child(date).get().val()
    if(log != None):
      update.message.reply_text(json.dumps(log, indent=4))
    else:
      update.message.reply_text("Invalid date entered or entered date not registered in firebase, Try again with format '7 December 2021'")
  else:
    update.message.reply_text("No date entered, Try again with format '7 December 2021'")

def updatetimings(update: Update, context: CallbackContext):
  lst = update.message.text.split(' ')[1:]
  if(len(lst) % 2 == 0):
    for i in range(0, len(lst), 2):
      db.child('timings').update({lst[i] : int(lst[i + 1])})
      update.message.reply_text("Updated index " + lst[i] + " with " + lst[i+1] + " successfully")
  else:
     update.message.reply_text("Passed arguments are not sufficient, try again!")

def unknown(update, context):
  user = str(update.effective_chat.id)
  if user == userId:
      context.bot.send_message(chat_id = user, text="Sorry, I didn't understand that command.")
  else:
      context.bot.send_message(chat_id = user, text="Hey! This bot is not for you")

firebase = pyrebase.initialize_app(firebaseConfig)
db = firebase.database()

def telegramBotInit():
  updater.dispatcher.add_handler(CommandHandler('start', start, filters = Filters.user(user_id = int(userId))))
  updater.dispatcher.add_handler(CommandHandler('showlog', showLog, filters = Filters.user(user_id = int(userId))))
  updater.dispatcher.add_handler(CommandHandler('update', updatetimings, filters = Filters.user(user_id = int(userId))))
  updater.dispatcher.add_handler(CommandHandler('pirlog', pirLog, filters = Filters.user(user_id = int(userId))))
  updater.dispatcher.add_handler(CommandHandler('index', indexKeyHelp, filters = Filters.user(user_id = int(userId))))
  updater.dispatcher.add_handler(MessageHandler(Filters.text, unknown))
  updater.dispatcher.add_handler(MessageHandler(Filters.command, unknown)) # Filters out unknown commands
  updater.dispatcher.add_handler(MessageHandler(Filters.text, unknown_text))
  updater.start_polling()

def deviceOffline(startTime):
  OfflineStatus = db.child("Offline").get().val()
  if(not OfflineStatus == 1): # if device is not offline
    deviceTime = db.child("time").get().val() # checking device time
    if(OfflineStatus == 2): # device just started
      sendToTelegram('Device online at ' + startTime)
      db.child().update({"Offline" : 0})
    H1, M1, S1 = map(int, deviceTime.split(":"))
    timeDiff = datetime.now() - timedelta(hours = H1, minutes = M1, seconds = S1)
    S = int(str(timeDiff).split()[1].split(':')[-2].split('.')[0])
    H, M = map(int, str(timeDiff).split()[1].split(':')[:2])
    t = timedelta(hours = H, minutes = M, seconds = S)
    print(datetime.now(), H, M, S, t, t.total_seconds())
    min = t.total_seconds() / 60
    if(min >= 2 and min <= 10):
      sendToTelegram("Device offline, Please check and connect ASAP\n Last seen at " + deviceTime)
      db.child().update({"Offline" : 1})

def motionDetection(date):
  tempPirCount = db.child("tempPirCount").get().val()
  deviceTime = db.child("time").get().val()
  #time = str(datetime.now()).split()[1][:5]
  h = int(deviceTime.split(':')[0])
  count = db.child("pirStatus").child(date).child(h).get().val()
  if(count == 0):
    tempPirCount = 0
  tempPirCount += 1
  db.child("pirStatus").child(date).update({h : tempPirCount})
  db.child().update({"tempPirCount" : tempPirCount})

def getDay(s):
  return int(s.split()[0])

def getMonth(s):
  return s.split()[1]

def getNextDate(date):
  months = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"]
  m = months.index(date.split()[1])
  nextD = str(datetime(int(date.split()[-1]), m + 1, int(date.split()[0])) + timedelta(days = 1)) 
  nexty = nextD.split()[0].split('-')[0]
  nextm = nextD.split()[0].split('-')[1]
  nextd = nextD.split()[0].split('-')[2]
  return nextd + ' ' + months[int(nextm) - 1] + ' ' + nexty

def checkStatus():
  while(True):
    timeNow = str(datetime.now()).split()[1][:5]
    H = int(timeNow.split(":")[0])
    M = int(timeNow.split(":")[1])
    pirst = db.child("pirTstatus").get().val()
    if(H == 7):
      if(pirst == -1):
        log = db.child("pirStatus").child(date).get().val()
        if(log != None):
          sendToTelegram(str(json.dumps(log, indent=4)))
        db.update({"pirTstatus": 1})
    else:
      db.update({"pirTstatus": -1})
    SOS = db.child("SOS").get().val()
    pirStatus = db.child("PIR").get().val()
    startTime = db.child("startTime").get().val()
    firebaseStatusInNode = db.child("status").get().val()
    status = db.child("time").get().val()
    currentTime = str(datetime.now()).split()[-1][:5]
    date = db.child("date").get().val()
    morningTelegramAlert = db.child(date).child(3).get().val()
    afternoonTelegramAlert = db.child(date).child(4).get().val()
    nightTelegramAlert = db.child(date).child(5).get().val()
    firebaseKeys = db.child().get()
    dict_keys = list(firebaseKeys.val().keys())
    if(H == 0 and (M >= 0 and M <= 5)):
      db.child("pirStatus").update({getNextDate(date) : [0] * 24})
    for key in dict_keys:
      if(len(key.split()[-1]) == 4 and len(key.split()) == 3):
        currentDay = getDay(date)
        currentMonth = getMonth(date)
        keyDate = getDay(key)
        keyMonth = getMonth(key)
        if(currentDay - keyDate >= 3 and currentMonth == keyMonth):
          db.child(key).remove()
        elif (currentMonth != keyMonth and currentDay >= 3):
          db.child(key).remove()

    pirStatusKeys = db.child("pirStatus").get()
    dict_keys = list(pirStatusKeys.val().keys())
    for key in dict_keys:
      if(len(key.split()[-1]) == 4 and len(key.split()) == 3):
        currentDay = getDay(date)
        currentMonth = getMonth(date)
        keyDate = getDay(key)
        keyMonth = getMonth(key)
        if(currentDay - keyDate >= 3 and currentMonth == keyMonth):
          db.child("pirStatus").child(key).remove()
        elif (currentMonth != keyMonth and currentDay >= 3):
          db.child("pirStatus").child(key).remove()
    deviceOffline(startTime)
    if(pirStatus):
        motionDetection(date)
        db.update({"PIR": False})
    if(firebaseStatusInNode == 404):
        sendToTelegram("Firebase error in NodeMCU")
        db.update({"status": 0})
    if(SOS):
        sendToTelegram("Got an SOS alert from Food and Medicine Remainder using ESP8266")
        db.update({"SOS": False})
    if(morningTelegramAlert == 2):
        sendToTelegram("Missed morning tablets")
        db.child(date).update({3 : -1})
    if(afternoonTelegramAlert == 2):
        sendToTelegram("Missed afternoon tablets")
        db.child(date).update({4 : -1})
    if(nightTelegramAlert == 2):
        sendToTelegram("Missed night tablets")
        db.child(date).update({5 : -1})
    if(morningTelegramAlert == 1):
        t = db.child(date).child(6).get().val()
        sendToTelegram("Took morning tablets at " + t)
        db.child(date).update({3 : -1})
    if(afternoonTelegramAlert == 1):
        t = db.child(date).child(7).get().val()
        sendToTelegram("Took afternoon tablets at " + t)
        db.child(date).update({4 : -1})
    if(nightTelegramAlert == 1):
        t = db.child(date).child(8).get().val()
        sendToTelegram("Took night tablets at " + t)
        db.child(date).update({5 : -1})
    time.sleep(5)


if __name__ == '__main__':

    try:
        t1 = threading.Thread(target = checkStatus)
        t1.start()
    except Exception as e:
        print(str(e))

    telegramBotInit()

