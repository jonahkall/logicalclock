import time

def EnforceRateLimit(ticksPerSecond):
  timeInterval = 1.0 / ticksPerSecond
  def decorate(function):
    lastCall = [0]
    def rateLimitedFunction(*args, **kargs):
      waitingTime = timeInterval - (time.clock() - lastCall[0])
      if waitingTime > 0:
        time.sleep(waitingTime)
      returnValue = function(*args, **kargs)
      lastCall[0] = time.clock()
      return returnValue
    return rateLimitedFunction
  return decorate
