(if (nil? (dyn *flychecking*))
  (import _libmpsse :prefix "" :export true))
(defdyn *ft-err* "Error status dynamic binding. Represents FT_STATUS as a Janet keyword (`:ok`)")