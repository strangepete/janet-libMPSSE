(if (nil? (dyn *flychecking*))
  (import _libmpsse :prefix "" :export true))
(defdyn *ft-err*)