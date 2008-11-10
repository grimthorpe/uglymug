import re
import urllib
import urllib2
import robotparser
import HTMLParser
import SOAPpy

class TitleFinder(HTMLParser.HTMLParser):
  def __init__(self):
    HTMLParser.HTMLParser.__init__(self)
    self.intitle=False
    self.title=''

  def handle_starttag(self, tag, attrs):
    if tag.lower() == 'title':
      self.intitle = True

  def handle_endtag(self, tag):
    if tag.lower() == 'title':
      self.intitle=False
      raise HTMLParser.HTMLParseError('Found Title')

  def handle_data(self, data):
    if self.intitle:
      self.title = self.title + ' ' + data.strip().replace('\n', '').replace('\r', '')

class shorturl:
  def __init__(self, longurl, shorturl, title):
    self.longurl = longurl
    self.shorturl = shorturl
    self.title = title

class shorten:
  def __init__(self):
    urllib.URLopener.version = 'Mozilla/4.0 (compatible; MSIE 5.5; Windows NT 5.0; T312461)'

  def findtitle(self, target):
    targethtml = TitleFinder()
    try:
      try:
        targethtml.feed(target.read())
        targethtml.close()
      except HTMLParser.HTMLParseError:
        pass
      print "Title: " + targethtml.title
      return targethtml.title
    except UnicodeDecodeError:
      return "ERROR: Non-ascii characters in title"

  def url(self, url):
    title=''
    try:
      short=urllib.urlopen("http://metamark.net/api/rest/simple", urllib.urlencode({"long_url":url})).read()
      if short.startswith("http://"):
        req_headers = { 'User-Agent': 'Mozilla/4.0 (compatible; MSIE 5.5; Windows NT)', }
        req = urllib2.Request(url, None, req_headers)
        target = urllib2.urlopen(req)
        if target.info()['Content-Type'].startswith('text/html'):
          title=self.findtitle(target)
          if title.startswith('ERROR'):
            title=''
        else:
          print "Can't hand Content-Type: " + target.info()['Content-Type']
      else:
        print 'Error from metamark: ' + short
        short="ERROR"
      return shorturl(url, short, title)
    except:
      return shorturl("ERROR", "ERROR", "")

