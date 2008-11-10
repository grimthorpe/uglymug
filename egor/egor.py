#!/usr/bin/env python
import sys
import string
import codecs
import re
import SOAPpy
import google
import os.path
import enchant
import random
import pickle
import socket
import time

from uglymug import uglymugsocket, linehandler
from shorten import shorten
from urllib import urlopen
from yahoo.search import factory


class TrustOrIgnore:
  def __init__(self):
    self.___trust = {}
    self.___ignore = {}
    self.trust('#-1', '#-1')

  def ___addtolist(self, addition, remove, who, why):
    if not who in addition:
      addition[who] = why
      try:
        del remove[who]
      except KeyError:
        pass

  def DoI(self, who, what):
    return who in what

  def DoITrust(self, who):
    return self.DoI(who, self.___trust)

  def DoIIgnore(self, who):
    return self.DoI(who, self.___ignore)

  def trust(self, who, why):
    print "Trusting " + who + " because of " + why
    return self.___addtolist(self.___trust, self.___ignore, who, why)

  def ignore(self, who, why):
    print "Ignoring " + who + " because of " + why
    return self.___addtolist(self.___ignore, self.___trust, who, why)

  def reset(self, who, why):
    try:
      del self.___ignore[who]
    except:
      pass
    try:
      del self.___trust[who]
    except:
      pass

class bot:
  def __init__(self, server, port):
    self.server=server
    self.wtffile = "./wtf_" + server + ".txt"
    self.shortenedfile = "./shortened_" + server + ".pickle"
    self.shortened={}
    self.um = uglymugsocket(server, port)
    self.input = linehandler()
    self.urlmatch=re.compile('(https?://[^ "\\(\\)<>]*(?: = [^ "\\(\\)<>]*)?)')
    self.shorten = shorten()
    self.magiccode='potrezebie'
    self.magic='***' + self.magiccode + '___:'
    self.magicmatch=re.compile(':(.*)<(.*)>')
    self.colonsplit=re.compile('(.*):(.*)')
    self.myid=''
    self.myname=''
    self.trust = TrustOrIgnore()
    self.dict = enchant.Dict("en_GB")
    self.unpickleshortened()

  def unpickleshortened(self):
    if os.path.exists(self.shortenedfile):
      file=open(self.shortenedfile, "r")
      self.shortened=pickle.load(file)
      file.close()

  def pickleshortened(self):
    file=open(self.shortenedfile, "w")
    pickle.dump(self.shortened, file)
    file.close()

  def responderror(self, who, command):
    if(command == 'tell' or command == 'page'):
      self.um.sendlineescaped("|" + command + " " + who + "=I can't shorten that url")
    elif(command == '@natter' or command == 'chat'):
      pass
    else:
      self.um.sendline("|say I'm sorry but I can't shorten {@?name #" + who + "}'s url")

  def respondsuccess(self, who, shorturl, title, command):
    if(command == 'tell' or command == 'page'):
      self.um.sendline("|" + command + " " + who + "=" + shorturl + title)
    elif(command == '@natter' or command == '@welcomer'):
      self.um.sendline(command + " :shortens {@?name #"+who+"}'s url to "+shorturl)
    else:
      self.um.sendline('|emote shortens {@?name #' + who + "}'s url to " +shorturl + title)

  def shortenurl(self, who, text, command):
    tooshort=0
    retval={}
    for url in self.urlmatch.findall(text):
      url=url.replace(" = ", "=")
      if(len(url) >= 20):
        if not self.shortened.has_key(url):
          self.shortened[url]={"url":self.shorten.url(url), "who":who}
          self.pickleshortened()
        short = self.shortened[url]
        if short["url"].shorturl.startswith("ERROR"):
          self.responderror(who, command)
        else:
          if short["url"].title != '':
            title = " - " + short["url"].title
            for search in [ '\\', '$', '{', '\n' ]:
              title = title.replace(search, '\\' + search)
          else:
            title=""
          self.respondsuccess(who, short["url"].shorturl, title, command)

  def magiccommand(self, command):
    self.um.sendline('@echo ' + self.magic + command)

  def doignore(self, result):
    for (existing, new) in self.colonsplit.findall(result):
      if new != 'Error':
        if self.trust.DoITrust(existing):
          self.trust.ignore(new, existing)
          if existing != '#-1':
            self.um.sendline(':now ignores {@?name ' + new + '}')

  def doforget(self, what):
    url=what
    if not url.startswith("http:"):
      url="http://" + what
    for (entry) in self.shortened:
      if entry["url"] == url:
        self.shortened.remove(entry)
        return
 
  def dotrust(self, result):
    for (existing, new) in self.colonsplit.findall(result):
      if new != 'Error':
        if self.trust.DoITrust(existing):
          self.trust.trust(new, existing)
          if existing != '#-1':
            self.um.sendline(':now trusts {@?name ' + new + '}')

  def doreset(self, result):
    for (existing, new) in self.colonsplit.findall(result):
      if new != 'Error':
        if self.trust.DoITrust(existing):
          self.trust.reset(new, existing)
          self.um.sendline(':now does not trust {@?name ' + new + '}, but wont ignore them either')

  def handlemagic(self, text):
    for (type, result) in self.magicmatch.findall(text):
      if type=='ID':
        self.myid=result
        print 'My #ID is ' + result
      elif type=='NAME':
        self.myname=result.lower()
        print 'My NAME is ' + result
      elif type=='TRUST':
        self.dotrust(result)
      elif type=='IGNORE':
        self.doignore(result)
      elif type=='RESET':
        self.doreset(result)

  def sendgetmyid(self):
    self.magiccommand('ID<{@?id me}>')

  def sendgetmyname(self):
    self.magiccommand('NAME<{@?name me}>')

  def sendgetTRUST(self, type, existing, new):
    if new == 'me':
      new = existing
    self.magiccommand(type + '<'+existing+':{@?id *'+new+'}>')
    
  def sendgettrust(self, existing, new):
    self.sendgetTRUST('TRUST', existing, new)

  def sendgetignore(self, existing, new):
    self.sendgetTRUST('IGNORE', existing, new)

  def sendgetreset(self, existing, new):
    self.sendgetTRUST('RESET', existing, new)

  def doyahoo(self, who, what):
    what=" ".join(what)
    self.um.sendline("|pose yodells '" + what + "'")
    print 'Yahooing for ', what
    search=factory.create_search('web', 'BBE4VALV34GMvB9exj83Yh84_qfPft6vEV0qq9MHIMy1OPjzEjcu2_BUQkb9_Wp0Bp1IxQ--')
    search.query = what;
    results = search.parse_results()
    resultstring=''
    for result in results:
      if resultstring != '':
        resultstring = resultstring +"\\\n"
      resultline="%(url)s - %(title)s" %{'url':result.Url, 'title':result.Title}
      resultstring=resultstring + resultline
      print resultline
    self.um.sendline(":gets a response.\\\n"+resultstring)

  def doweather(self, who, what):
    what="".join(what)
    self.um.sendline("|pose tries to find the weather for {@?name "+who+"}.")
    weather=urlopen("http://search.weather.yahoo.com/search/weather2?p="+what).read();
    print weather

  def dogoogle(self, who, what):
    what=" ".join(what)
    self.um.sendline("|pose looks up '" +what + "' for {@?name "+who+"}.")
    print 'Googling for ', what
    try:
      data = google.doGoogleSearch(what)
      if data.meta.estimatedTotalResultsCount > 0:
        title=data.results[0].title
        url=data.results[0].URL
        snippet=data.results[0].snippet
        title=title.replace("<b>", "%h")
        title=title.replace("</b>", "%z")
        title=title.replace("&amp;", "&")
        self.um.sendline(":gets a response.\\\n"+url + " - "+title + "\n")
      else:
        self.um.sendlineescaped("|say Google can't find anything about that.")
    except SOAPpy.Errors.HTTPError:
      self.um.sendlineescaped("|say Something went wrong there - I can't get through at the moment. Why don't you try again in a couple of minutes")

  def dowtf(self, who, what):
    WTF = ["./wtf.txt", self.wtffile, "/usr/share/games/bsdgames/acronyms", "/usr/share/games/bsdgames/acronyms.comp"]
    word=""
    if len(what) > 1:
      if what[0] == "is":
        word=what[1]
      else:
        word=what[0]
    else:
      word=what[0]
    if word=="":
      self.um.sendlineescaped("|say What did you want to know?")
      return
    word=word.upper()
    found=False
    for wtf in WTF:
      if os.path.exists(wtf):
        file = open(wtf, "r")
        for line in file:
          acronym=line.split("	")
          if len(acronym) == 2:
            if acronym[0] == word:
              found=True
              if wtf==self.wtffile:
                self.um.sendline("|say I've been told that '" + word +"' means " +acronym[1])
              else:
                self.um.sendlineescaped("|say I think '" + word + "' means " + acronym[1])
        file.close()
    if not found:
      self.um.sendlineescaped("|say I'm sorry, but I don't know what " + word + " means")

  def dolearn(self, who, what):
    if len(what) > 1:
      word=what[0]
      definition=" ".join(what[1:]).replace("\\", "\\\\").replace("$", "\\$").replace("{", "\\{")
      file = open(self.wtffile, "a")
      file.write(word.upper() + "\t" + definition + " (by {@?name "+who+"})\n")
      file.close()
      self.um.sendlineescaped("|say Ok, I'll remember that for next time someone asks me.")

  def dospell(self, who, what):
    self.um.sendline("|pose searches through his dictionary for {@?name "+who+"}.")
    ok=True
    for word in what:
      if not self.dict.check(word):
        ok=False
        suggestlist=self.dict.suggest(word)
        list=[]
        for check in suggestlist:
          if check.endswith("'s"):
            pass
          elif check == check.capitalize():
            pass
          else:
            list.append(check)
        if len(list) != 0:
          if len(list) > 1:
            list=", ".join(list[:-1]) + " or " + list[-1]
          else:
            list=list[0]
          self.um.sendline("|say for '" + word + "' try " + list)
        else:
          self.um.sendline("|say I can't even work out what '" + word + "' could be")
    if ok:
      self.um.sendline("|say I can't see anything wrong with that.")

  def dosillyspell(self, who, what):
    reply=""
    for word in what:
      reply=" ".join([reply, random.sample(self.dict.suggest(word), 1)[0]])
    self.um.sendlineescaped("|say " + reply)

  def dochat(self, who, command, text):
    pass

  def dorank(self, what):
    arg = ""
    maxlines = "1"
    for param in what:
      if param.startswith("-"):
        maxlines=param.strip("-")
      else:
        arg=arg+" " + param
    if not maxlines.isdigit():
      maxlines = "1"
    arg = arg + " | tail -"+maxlines
    print "RANK: " + arg
    rank = os.popen("/home/adrian/mudconnect/analyse " + arg)
    lines=""
    for line in rank.readlines():
      line = line.replace("\n", "")
      print line
      if(lines != ""):
        lines = lines + "\\\n"
      lines = lines + line.replace("\\", "\\\\").replace("$", "\\$").replace("{", "\\{").replace("%", "%%")
    self.um.sendline("|say MudConnect ranking as asked for:\\\n"+lines)

  def getfortune(self, what, mergelines):
    fortune = os.popen("/usr/games/fortune " + what)
    lines=""
    for line in fortune.readlines():
      line = line.replace("\n", "")
      print line
      if(lines != ""):
        if((not mergelines) or line.startswith(" ")):
          lines = lines + "\\\n"
        else:
          lines = lines + " "
      lines = lines + line.replace("\\", "\\\\").replace("$", "\\$").replace("{", "\\{").replace("%", "%%")
    return lines

  def doboot(self, who, target):
    self.um.sendline("boot " + target)
    self.um.sendline("boot " + target  +" = " + self.getfortune('fortunes', True))

  def dofortune(self, who, title, what, mergelines):
    self.um.sendline("|emote reads {@?name " + who + "}'s "+title+":\\\n"+self.getfortune(what, mergelines))

  def checkforcommand(self, who, text, command):
    if self.trust.DoIIgnore(who):
      return
    for i in [ "'", '"', ',', "!", "?", "{", "$", "}" ]:
      text = text.replace(i, ' ')
    text = text.replace('  ', ' ')
    words = text.split()
    if (command=='chat' or command=='@natter' or command=='@welcomer'):
      self.dochat(who, command, text)
      return
    for count in range(0, len(words) - 1):
      word=words[count].lower()
      if word==self.myname:
        nextword=words[count + 1].lower()
        if nextword == 'rank':
          self.dorank(words[count + 2:])
          return
        elif count < len(words) - 2:
          if self.trust.DoITrust(who):
            if nextword == 'trust':
              self.sendgettrust(who, words[count + 2])
              return
            if nextword == 'ignore':
              self.sendgetignore(who, words[count + 2])
              return
            if nextword == 'reset':
              self.sendgetreset(who, words[count + 2])
              return
            if nextword == 'boot':
              self.doboot(who, words[count + 2])
              return
            if nextword == 'do':
              self.um.sendline(" ".join(words[count+2:]))
              return
            if nextword == 'forget':
              self.doforget(words[count+2])
              return
          if nextword == 'yahoo':
            self.doyahoo(who, words[count + 2:])
            return
          if nextword == 'google':
            self.dogoogle(who, words[count + 2:])
            return
          if nextword == '     *************weather':
            self.doweather(who, words[count + 2:])
            return
          if nextword == 'spell':
            self.dospell(who, words[count + 2:])
            return
          if nextword == 'sillyspell':
            self.dosillyspell(who, words[count + 2:])
            return
          if nextword == 'wtf' or nextword == 'what':
            self.dowtf(who, words[count + 2:])
            return
          if nextword == 'learn':
            self.dolearn(who, words[count + 2:])
            return
        else:
          if nextword == 'fortune':
            self.dofortune(who, nextword, "fortunes", True)
            return
          if nextword == 'insult':
            self.dofortune(who, nextword, "./jokes", False)
            return
          if nextword == 'joke':
            self.dofortune(who, nextword, "riddles", False)
            return
          if nextword == 'bofh':
            self.dofortune(who, nextword, "bofh-excuses", False)
            return

  def parsetext(self, who, text, command):
    if self.trust.DoIIgnore(who):
      return
    print (who, text, command)
    if (text.find('http://') > -1) or (text.find('https://') > -1):
      self.shortenurl(who, text, command)
    elif text.lower().find(self.myname):
      self.checkforcommand(who, text, command)

  def parse(self, line):
    if line.who != '-1':
      if line.command=='@echo':
        if line.text.startswith(self.magic):
          self.handlemagic(line.text)
      elif self.myid!='':
        if line.who != self.myid:
          self.parsetext(line.who, line.text, line.command)
        else:
          print ("-->", line.text)

  def run(self, name, password, controller):
    self.um.connect()
    self.um.sendline(name)
    self.um.sendline(password)
    self.um.sendline('N')
    self.um.sendline('pub')
    self.um.sendline('@set me = !colour')
    self.um.sendline('ansafone clear')
    self.um.sendline('ansafone off')
    self.um.sendline('@spam on')
    self.um.sendline('@terminal halfquit=on')
    self.um.sendline('@terminal commandinfo=on')
    self.sendgetmyid()
    self.sendgetmyname()
    self.sendgettrust('#-1', controller)
    self.sendgetignore('#-1', 'spamboy')
    self.um.sendline('chat #egor')

    fullline=''
    while not self.um.abort:
      line=self.um.readline()
      if not self.um.abort:
        if line!='':
          self.input.decode(line)
          self.parse(self.input)
        else:
          self.um.sendline('IDLE')

if __name__ == '__main__':
  sys.stdout=codecs.lookup('utf-8')[-1](sys.stdout)
  name='NAME'
  password='PASSWORD'
  controller='grimthorpe'
  server='uglymug.org.uk'
  port='6239'
  startupurl=''
  if len(sys.argv) < 2:
    print "Usage: egor <username> <password> [<controller> [<mudserver> <port> [<startupurl>]]]"
  else:
    if len(sys.argv) > 2:
      name=sys.argv[1]
      password=sys.argv[2]
      if len(sys.argv) > 3:
        controller=sys.argv[3]
      if len(sys.argv) > 5:
        server=sys.argv[4]
        port=sys.argv[5]
      if len(sys.argv) > 6:
        startupurl=sys.argv[6]

    egor=bot(server, port)
    while 1:
      try:
        if startupurl != '':
          print "Accessing URL: " + startupurl
          urlopen(startupurl).read()
        egor.run(name, password, controller)
      except socket.error:
        print "Connection died. Retrying."
      except IOError:
        print "Failed, waiting 5 seconds then trying again."
        time.sleep(5)
