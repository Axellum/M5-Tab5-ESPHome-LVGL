// Bouchons du rendu hors tablette (tab5-rendu-host.yaml, lot 7 de l'audit « ouverture »).
// Le matériel audio et l'assistant vocal du Tab5 n'existent pas sur la plateforme
// `host` : ces classes prennent leur place, sans rien faire, pour que l'interface
// compile et se dessine telle quelle. Jamais dans le firmware de la tablette.
#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_MEDIA_PLAYER
#include "esphome/components/media_player/media_player.h"
#endif
#ifdef USE_IMAGE
#include "esphome/components/image/image.h"
#endif

namespace esphome::rendu_muet {

// Toute action du matériel absent : ne fait rien, passe à la suivante.
template<typename... Ts> class RienAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override {}
};

// Toute condition du matériel absent : toujours fausse (rien ne tourne).
template<typename... Ts> class FauxCondition : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return false; }
};

// speaker et microphone d'ESPHome tirent le composant `audio`, réservé à l'ESP32 : ils
// sont remplacés (Tab5/rendu/composants/speaker, microphone), et leurs plateformes
// muettes n'ont qu'à répondre à ce que lisent les lambdas de l'interface.
class HautParleurMuet : public Component {
 public:
  bool is_running() const { return false; }
};

class MicroMuet : public Component {};

// rtttl : le réveil règle le volume de sa sonnerie avant de la jouer.
class SonnerieMuette : public Component {
 public:
  void set_gain(float gain) {}
};

#ifdef USE_MEDIA_PLAYER
class LecteurMuet : public media_player::MediaPlayer, public Component {
 public:
  media_player::MediaPlayerTraits get_traits() override { return {}; }
  void control(const media_player::MediaPlayerCall &call) override {
    if (call.get_volume().has_value())
      this->volume = *call.get_volume();
    this->publish_state();
  }
};
#endif

// voice_assistant : les lambdas de l'interface lisent seulement ces deux états.
class AssistantMuet : public Component {
 public:
  bool is_running() const { return false; }
  bool is_continuous() const { return false; }
};

// micro_wake_word : rien à lire, seulement un id à référencer.
class ReveilMuet : public Component {};

#ifdef USE_IMAGE
// Image téléchargée (online_image) : un pixel noir, jamais remplacé.
class ImageMuette : public image::Image {
 public:
  ImageMuette() : image::Image(PIXEL, 1, 1, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE) {}

 protected:
  inline static constexpr uint8_t PIXEL[2] = {0, 0};
};
#endif

}  // namespace esphome::rendu_muet
