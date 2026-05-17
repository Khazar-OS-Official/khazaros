#ifndef POWER_H
#define POWER_H

#include <libk/types.h>

// Sistem enerjisini kapat / yeniden başlatma için basit arayüz.
// Gerçek PC və VM-lərdə mümkün qədər çox senaryonu dəstəkləmək üçün
// bir neçə metod sınanır, uğursuz olarsa CPU HLT döngüsünə alınır.

// Tam sistem shutdown (ACPI/APM portlarına siqnal + fallback HLT döngüsü)
void power_shutdown(void);

// Sistem reboot (8042 klavye kontrolcüsü üzerinden reset siqnalı)
void power_reboot(void);

#endif // POWER_H

