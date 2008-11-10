import re
import telnetlib
import codecs
import socket

class uglymugsocket:
  def __init__(self, server, port):
    self.abort=1
    self.server=server
    self.port=port
    self.encoder=codecs.getencoder('utf_8')

  def connect(self):
    self.sock=telnetlib.Telnet(self.server, self.port)
    self.buffer=''
    self.abort=0

  def sendline(self, line):
    (line, amount) = self.encoder(line + '\n\n')
    self.sock.write(line)

  def sendlineescaped(self, line):
    line = line.replace("\\", "\\\\").replace("$", "\\$").replace("{", "\\{")
    self.sendline(line)

  def readline(self):
    try:
      retval = self.sock.read_until('\n', 300).replace('\n', '')
    except EOFError:
      self.abort=1
      return ''
    except socket.error:
      self.abort=1
      return ''
    return retval.replace('\r', '')

class linehandler:
  def __init__(self):
    self.lineregexp='\x01\\(([^\x02]*)\\)\x02\\((#-1|#[0-9]+)\\)\x03(.*)'
    self.commandmatch=re.compile(self.lineregexp, re.DOTALL)
    self.who=''
    self.command=''
    self.text=''

  def decode(self, x):
    params = self.commandmatch.findall(x)
    if len(params) > 0:
      (self.command, self.who, self.text) = params[0]
    else:
      self.text=x

