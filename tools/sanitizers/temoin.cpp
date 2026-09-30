// Témoin positif du job sanitizers : un comportement indéfini connu (conversion d'un
// flottant hors des bornes de `int`, le défaut corrigé au lot A de l'audit du
// 30/09/2026), compilé avec les MÊMES drapeaux que la tablette virtuelle. Si
// tools/sanitizers/rapports.py ne trouve pas son rapport, la chaîne de détection est
// cassée et un « 0 rapport » de la tablette ne prouverait rien.
#include <cstdio>

int main(int argc, char**) {
    volatile float f = argc > 0 ? 1e30f : 0.0f;
    volatile int i = (int) f;
    std::printf("%d\n", i);
    return 0;
}
