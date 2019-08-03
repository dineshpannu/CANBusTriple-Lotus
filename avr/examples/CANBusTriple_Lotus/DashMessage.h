#ifndef DashMessage_H
#define DashMessage_H

struct DashMessage 
{
  byte speed; // adjusted speed ~= d(XXh)-11d (61h-->97-11=86 mph) -- FF should be 256kmh
  byte rpm_1; // tach rpms [d(CCh)*256]+d(DDh) 06 D2 = 1746 rpm -- h27 should be 10,000rpm
  byte rpm_2; //  
  byte fuel; // fuel level (00=empty, FF=full) d[5] / 256 * 100 -- fuel %
  byte temperature; // engine temperature ~= d(XXh)-14d (D0-->208-14=194F)
  byte mil; // MIL 06-on, 04-crank, 00-running, 01-shift light
};

#endif
