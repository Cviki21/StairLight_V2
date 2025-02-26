document.addEventListener("DOMContentLoaded", () => {
  // Translation Dictionary
  const translations = {
    en: {
      title: "StairLight Configuration",
      led1Settings: "LED1 Settings",
      led2Settings: "LED2 Settings",
      ledType: "LED Type:",
      off: "Off",
      installationMode: "Installation Mode:",
      linija: "Line",
      stepenica: "Step",
      numberOfLEDs: "Number of LEDs:",
      numberOfSteps: "Number of Steps:",
      defaultLEDsPerStep: "Default LEDs per Step:",
      variableStepLength: "Variable Step Length",
      efektKreniOd: "Effect Start From:",
      sredine: "Middle",
      ruba: "Edge",
      rotation: "Rotation: 180° every other",
      stepMode: "Step Mode:",
      allAtOnce: "All At Once",
      sequence: "Sequence",
      effect: "Effect:",
      solid: "Solid",
      confetti: "Confetti",
      rainbow: "Rainbow",
      meteor: "Meteor",
      fade: "Fade In/Out",
      fire: "Fire",
      lightning: "Lightning",
      color: "Color:",
      colorOrder: "Color Order:",
      wipeSpeed: "Wipe In/Out Speed (ms):",
      onTime: "On Time (sec):",
      preview: "Preview:",
      mainSwitch: "Main Switch:",
      maxCurrent: "Max Current (mA):",
      saveSettings: "Save Settings",
      welcome: "Welcome to StairLight",
      introText: "Configure your smart lighting system with ease!",
      gettingStarted: "Getting Started",
      step1: "Press and hold the main switch for 6-8 seconds to activate Access Point (AP) mode.",
      step2: "Connect to WiFi network \"StairLight_AP\" (password: 12345678).",
      step3: "Navigate to 192.168.4.1 in your browser to access this page.",
      step4: "Use the LED1 and LED2 tabs to adjust settings."
    },
    hr: {
      title: "StairLight Konfiguracija",
      led1Settings: "Postavke LED1",
      led2Settings: "Postavke LED2",
      ledType: "Tip LED-a:",
      off: "Isključi",
      installationMode: "Način instalacije:",
      linija: "Linija",
      stepenica: "Stepenica",
      numberOfLEDs: "Broj LED-ova:",
      numberOfSteps: "Broj stepenica:",
      defaultLEDsPerStep: "Zadani broj LED po stepenici:",
      variableStepLength: "Promjenjiva širina stepenica",
      efektKreniOd: "Efekt Kreni Od:",
      sredine: "Sredine",
      ruba: "Ruba",
      rotation: "Rotacija svake druge za 180°",
      stepMode: "Način paljenja:",
      allAtOnce: "Sve odjednom",
      sequence: "Sekvenca",
      effect: "Efekt:",
      solid: "Solid",
      confetti: "Konfeti",
      rainbow: "Duga",
      meteor: "Meteor",
      fade: "Fade In/Out",
      fire: "Vatra",
      lightning: "Munja",
      color: "Boja:",
      colorOrder: "Redoslijed boja:",
      wipeSpeed: "Brzina brisanja (ms):",
      onTime: "Vrijeme uključenja (sec):",
      preview: "Pregled:",
      mainSwitch: "Glavni Prekidač:",
      maxCurrent: "Maksimalna struja (mA):",
      saveSettings: "Spremi Postavke",
      welcome: "Dobrodošli u StairLight",
      introText: "Jednostavno konfigurirajte svoj pametni sustav osvjetljenja!",
      gettingStarted: "Kako početi",
      step1: "Pritisnite i držite glavni prekidač 6-8 sekundi da aktivirate Access Point (AP) način.",
      step2: "Povežite se na WiFi mrežu \"StairLight_AP\" (lozinka: 12345678).",
      step3: "U pregledniku idite na 192.168.4.1 za pristup ovoj stranici.",
      step4: "Koristite kartice LED1 i LED2 za podešavanje postavki."
    }
  };

  let currentLang = "en";

  function updateTranslations() {
    const elements = document.querySelectorAll("[data-translate]");
    elements.forEach(el => {
      const key = el.getAttribute("data-translate");
      if (translations[currentLang] && translations[currentLang][key]) {
        el.textContent = translations[currentLang][key];
      }
    });
    document.title = translations[currentLang].title;
  }

  window.translate = function(key) {
    return translations[currentLang][key] || key;
  };

  updateTranslations();

  const langToggle = document.getElementById("langToggle");
  langToggle.addEventListener("click", () => {
    currentLang = currentLang === "en" ? "hr" : "en";
    langToggle.textContent = currentLang.toUpperCase();
    updateTranslations();
  });

  // Navigation
  const homePage = document.getElementById("homePage");
  const led1Page = document.getElementById("led1Page");
  const led2Page = document.getElementById("led2Page");
  const globalSettingsLed1 = document.getElementById("globalSettingsLed1");
  const globalSettingsLed2 = document.getElementById("globalSettingsLed2");
  const saveButtonLed1 = document.getElementById("saveConfigButtonLed1");
  const saveButtonLed2 = document.getElementById("saveConfigButtonLed2");

  function showPage(pageId) {
    [homePage, led1Page, led2Page].forEach(page => {
      page.style.display = "none";
    });
    globalSettingsLed1.classList.add("hidden");
    globalSettingsLed2.classList.add("hidden");
    saveButtonLed1.classList.add("hidden");
    saveButtonLed2.classList.add("hidden");

    const selectedPage = document.getElementById(pageId);
    selectedPage.style.display = "block";
    if (pageId === "led1Page") {
      globalSettingsLed1.classList.remove("hidden");
      saveButtonLed1.classList.remove("hidden");
    } else if (pageId === "led2Page") {
      globalSettingsLed2.classList.remove("hidden");
      saveButtonLed2.classList.remove("hidden");
    }
  }
  showPage("homePage");

  document.getElementById("homeNavButton").addEventListener("click", () => showPage("homePage"));
  document.getElementById("led1NavButton").addEventListener("click", () => showPage("led1Page"));
  document.getElementById("led2NavButton").addEventListener("click", () => showPage("led2Page"));

  // Helper: Toggle Visibility
  function toggleVisibility(el, show) {
    if (el) {
      el.classList.toggle("hidden", !show);
    }
  }

  // LED1 Configuration
  const led1Type = document.getElementById("led1Type");
  const led1ColorOrder = document.getElementById("led1ColorOrder");
  const led1ColorOrderSection = document.getElementById("led1ColorOrderSection");
  const led1ModeSection = document.getElementById("led1ModeSection");
  const led1InstallationMode = document.getElementById("led1InstallationMode");
  const led1LinijaSettings = document.getElementById("led1LinijaSettings");
  const led1StepenicaSettings = document.getElementById("led1StepenicaSettings");
  const led1NumSteps = document.getElementById("led1NumSteps");
  const led1DefaultLedsPerStep = document.getElementById("led1DefaultLedsPerStep");
  const led1VariableSteps = document.getElementById("led1VariableSteps");
  const led1DynamicSteps = document.getElementById("led1DynamicSteps");
  const led1Effects = document.getElementById("led1Effects");
  const led1Effect = document.getElementById("led1Effect");
  const led1ColorPicker = document.getElementById("led1Color");
  const led1EfektKreniOdSredina = document.getElementById("led1_efektKreniOd_sredina");
  const led1EfektKreniOdRuba = document.getElementById("led1_efektKreniOd_ruba");
  const led1RotDiv = document.getElementById("led1_rotDiv");
  const led1EffectPreview = document.getElementById("led1EffectPreview");
  const led1Brightness = document.getElementById("led1Brightness");
  const led1BrightnessVal = document.getElementById("led1BrightnessVal");

  led1Type.addEventListener("change", () => {
    const show = led1Type.value !== "" && led1Type.value !== "off";
    toggleVisibility(led1ModeSection, show);
    toggleVisibility(led1ColorOrderSection, show && led1Type.value !== "ws2811_white");
    if (!show) {
      toggleVisibility(led1LinijaSettings, false);
      toggleVisibility(led1StepenicaSettings, false);
      toggleVisibility(led1Effects, false);
    }
  });

  led1InstallationMode.addEventListener("change", () => {
    const mode = led1InstallationMode.value;
    if (mode === "linija") {
      toggleVisibility(led1LinijaSettings, true);
      toggleVisibility(led1StepenicaSettings, false);
      toggleVisibility(led1Effects, true);
    } else if (mode === "stepenica") {
      toggleVisibility(led1StepenicaSettings, true);
      toggleVisibility(led1LinijaSettings, false);
      toggleVisibility(led1Effects, true);
    } else {
      toggleVisibility(led1LinijaSettings, false);
      toggleVisibility(led1StepenicaSettings, false);
      toggleVisibility(led1Effects, false);
    }
  });

  led1Effect.addEventListener("change", () => {
    toggleVisibility(led1ColorPicker.parentElement, led1Effect.value === "solid");
    updateLed1EffectPreview();
  });

  led1ColorPicker.addEventListener("input", updateLed1EffectPreview);
  led1Brightness.addEventListener("input", () => { led1BrightnessVal.innerText = led1Brightness.value; });

  function updateLed1EfektKreniOd() {
    if (led1EfektKreniOdRuba.checked) {
      toggleVisibility(led1RotDiv, true);
    } else {
      toggleVisibility(led1RotDiv, false);
    }
  }
  led1EfektKreniOdSredina.addEventListener("change", updateLed1EfektKreniOd);
  led1EfektKreniOdRuba.addEventListener("change", updateLed1EfektKreniOd);
  updateLed1EfektKreniOd();

  function generateDynamicSteps(numSteps, defaultValue, container) {
    container.innerHTML = '';
    for (let i = 0; i < numSteps; i++) {
      const div = document.createElement('div');
      div.className = 'field';
      const label = document.createElement('label');
      label.textContent = `Step ${i + 1} LEDs:`;
      const input = document.createElement('input');
      input.type = 'number';
      input.className = 'step-input';
      input.min = '1';
      input.value = defaultValue;
      input.dataset.step = i;
      div.appendChild(label);
      div.appendChild(input);
      container.appendChild(div);
    }
  }

  led1VariableSteps.addEventListener("change", () => {
    if (led1VariableSteps.checked) {
      const numSteps = parseInt(led1NumSteps.value) || 0;
      const defaultVal = parseInt(led1DefaultLedsPerStep.value) || 0;
      if (numSteps > 0 && defaultVal > 0) {
        generateDynamicSteps(numSteps, defaultVal, led1DynamicSteps);
        toggleVisibility(led1DynamicSteps, true);
      } else {
        showPopup("Prvo unesite broj stepenica i LED-ova po stepenici!");
        led1VariableSteps.checked = false;
      }
    } else {
      toggleVisibility(led1DynamicSteps, false);
      led1DynamicSteps.innerHTML = "";
    }
  });

  led1NumSteps.addEventListener("change", () => {
    if (led1VariableSteps.checked) {
      const numSteps = parseInt(led1NumSteps.value) || 0;
      const defaultVal = parseInt(led1DefaultLedsPerStep.value) || 0;
      if (numSteps > 0 && defaultVal > 0) {
        generateDynamicSteps(numSteps, defaultVal, led1DynamicSteps);
      }
    }
  });

  led1DefaultLedsPerStep.addEventListener("change", () => {
    if (led1VariableSteps.checked) {
      const numSteps = parseInt(led1NumSteps.value) || 0;
      const defaultVal = parseInt(led1DefaultLedsPerStep.value) || 0;
      if (numSteps > 0 && defaultVal > 0) {
        generateDynamicSteps(numSteps, defaultVal, led1DynamicSteps);
      }
    }
  });

  function updateLed1EffectPreview() {
    let effect = led1Effect.value;
    if (effect === "solid") {
      led1EffectPreview.style.background = led1ColorPicker.value;
    } else if (effect === "rainbow") {
      led1EffectPreview.style.background = "linear-gradient(90deg, red, orange, yellow, green, blue, indigo, violet)";
    } else if (effect === "confetti") {
      led1EffectPreview.style.background = "repeating-linear-gradient(45deg, red, red 5px, green 5px, green 10px)";
    } else if (effect === "meteor") {
      led1EffectPreview.style.background = "linear-gradient(to right, transparent, yellow, transparent)";
    } else if (effect === "fade") {
      led1EffectPreview.style.background = "#ff69b4";
    } else if (effect === "fire") {
      led1EffectPreview.style.background = "linear-gradient(to top, #ff4500, #ff8c00, #ffd700)";
    } else if (effect === "lightning") {
      led1EffectPreview.style.background = "linear-gradient(to bottom, white, yellow, white)";
    } else {
      led1EffectPreview.style.background = "transparent";
    }
  }
  toggleVisibility(led1ColorPicker.parentElement, led1Effect.value === "solid");
  updateLed1EffectPreview();

  // LED2 Configuration
  const led2Type = document.getElementById("led2Type");
  const led2ColorOrder = document.getElementById("led2ColorOrder");
  const led2ColorOrderSection = document.getElementById("led2ColorOrderSection");
  const led2ModeSection = document.getElementById("led2ModeSection");
  const led2InstallationMode = document.getElementById("led2InstallationMode");
  const led2LinijaSettings = document.getElementById("led2LinijaSettings");
  const led2StepenicaSettings = document.getElementById("led2StepenicaSettings");
  const led2NumSteps = document.getElementById("led2NumSteps");
  const led2DefaultLedsPerStep = document.getElementById("led2DefaultLedsPerStep");
  const led2VariableSteps = document.getElementById("led2VariableSteps");
  const led2DynamicSteps = document.getElementById("led2DynamicSteps");
  const led2Effects = document.getElementById("led2Effects");
  const led2Effect = document.getElementById("led2Effect");
  const led2ColorPicker = document.getElementById("led2Color");
  const led2EfektKreniOdSredina = document.getElementById("led2_efektKreniOd_sredina");
  const led2EfektKreniOdRuba = document.getElementById("led2_efektKreniOd_ruba");
  const led2RotDiv = document.getElementById("led2_rotDiv");
  const led2EffectPreview = document.getElementById("led2EffectPreview");
  const led2Brightness = document.getElementById("led2Brightness");
  const led2BrightnessVal = document.getElementById("led2BrightnessVal");

  led2Type.addEventListener("change", () => {
    const show = led2Type.value !== "" && led2Type.value !== "off";
    toggleVisibility(led2ModeSection, show);
    toggleVisibility(led2ColorOrderSection, show && led2Type.value !== "ws2811_white");
    if (!show) {
      toggleVisibility(led2LinijaSettings, false);
      toggleVisibility(led2StepenicaSettings, false);
      toggleVisibility(led2Effects, false);
    }
  });

  led2InstallationMode.addEventListener("change", () => {
    const mode = led2InstallationMode.value;
    if (mode === "linija") {
      toggleVisibility(led2LinijaSettings, true);
      toggleVisibility(led2StepenicaSettings, false);
      toggleVisibility(led2Effects, true);
    } else if (mode === "stepenica") {
      toggleVisibility(led2StepenicaSettings, true);
      toggleVisibility(led2LinijaSettings, false);
      toggleVisibility(led2Effects, true);
    } else {
      toggleVisibility(led2LinijaSettings, false);
      toggleVisibility(led2StepenicaSettings, false);
      toggleVisibility(led2Effects, false);
    }
  });

  led2Effect.addEventListener("change", () => {
    toggleVisibility(led2ColorPicker.parentElement, led2Effect.value === "solid");
    updateLed2EffectPreview();
  });

  led2ColorPicker.addEventListener("input", updateLed2EffectPreview);
  led2Brightness.addEventListener("input", () => { led2BrightnessVal.innerText = led2Brightness.value; });

  function updateLed2EfektKreniOd() {
    if (led2EfektKreniOdRuba.checked) {
      toggleVisibility(led2RotDiv, true);
    } else {
      toggleVisibility(led2RotDiv, false);
    }
  }
  led2EfektKreniOdSredina.addEventListener("change", updateLed2EfektKreniOd);
  led2EfektKreniOdRuba.addEventListener("change", updateLed2EfektKreniOd);
  updateLed2EfektKreniOd();

  function updateLed2EffectPreview() {
    let effect = led2Effect.value;
    if (effect === "solid") {
      led2EffectPreview.style.background = led2ColorPicker.value;
    } else if (effect === "rainbow") {
      led2EffectPreview.style.background = "linear-gradient(90deg, red, orange, yellow, green, blue, indigo, violet)";
    } else if (effect === "confetti") {
      led2EffectPreview.style.background = "repeating-linear-gradient(45deg, red, red 5px, green 5px, green 10px)";
    } else if (effect === "meteor") {
      led2EffectPreview.style.background = "linear-gradient(to right, transparent, yellow, transparent)";
    } else if (effect === "fade") {
      led2EffectPreview.style.background = "#ff69b4";
    } else if (effect === "fire") {
      led2EffectPreview.style.background = "linear-gradient(to top, #ff4500, #ff8c00, #ffd700)";
    } else if (effect === "lightning") {
      led2EffectPreview.style.background = "linear-gradient(to bottom, white, yellow, white)";
    } else {
      led2EffectPreview.style.background = "transparent";
    }
  }
  toggleVisibility(led2ColorPicker.parentElement, led2Effect.value === "solid");
  updateLed2EffectPreview();

  // Modal Popup
  const popup = document.getElementById("popup");
  const closeBtn = document.querySelector(".close-button");
  const popupMsg = document.getElementById("popupMsg");

  closeBtn.addEventListener("click", () => { popup.style.display = "none"; });
  window.addEventListener("click", (ev) => { if (ev.target === popup) popup.style.display = "none"; });

  function showPopup(msg) {
    popupMsg.innerText = msg;
    popup.style.display = "block";
  }

  // Fetch and Save Configuration
  function fetchConfig() {
    console.log("Fetching config...");
    fetch("/api/getConfig")
      .then(res => {
        console.log("Response status:", res.status);
        if (!res.ok) throw new Error(`HTTP error! status: ${res.status}`);
        return res.json();
      })
      .then(configData => {
        console.log("Config received:", configData);

        // Global settings
        document.getElementById("mainSwitch").value = configData.mainSwitch || "both";
        document.getElementById("maxCurrent").value = configData.maxCurrent || 2000;

        // LED1
        led1Type.value = configData.led1?.type || "";
        led1ColorOrder.value = configData.led1?.colorOrder || "GRB";
        if (led1Type.value && led1Type.value !== "off") {
          toggleVisibility(led1ModeSection, true);
          toggleVisibility(led1ColorOrderSection, led1Type.value !== "ws2811_white");
          led1InstallationMode.value = configData.led1?.mode || "";

          if (led1InstallationMode.value === "linija") {
            toggleVisibility(led1LinijaSettings, true);
            document.getElementById("led1LinijaLEDCount").value = configData.led1?.linijaLEDCount || "";
            toggleVisibility(led1Effects, true);
          } else if (led1InstallationMode.value === "stepenica") {
            toggleVisibility(led1StepenicaSettings, true);
            led1NumSteps.value = configData.led1?.numSteps || "";
            led1DefaultLedsPerStep.value = configData.led1?.defaultLedsPerStep || "";
            led1VariableSteps.checked = configData.led1?.variable || false;

            if (configData.led1?.efektKreniOd === "ruba") {
              led1EfektKreniOdRuba.checked = true;
            } else {
              led1EfektKreniOdSredina.checked = true;
            }
            updateLed1EfektKreniOd();

            if (configData.led1?.stepMode === "sequence") {
              document.getElementById("led1_stepMode_sequence").checked = true;
            } else {
              document.getElementById("led1_stepMode_allAtOnce").checked = true;
            }

            if (led1VariableSteps.checked) {
              generateDynamicSteps(parseInt(led1NumSteps.value) || 0, parseInt(led1DefaultLedsPerStep.value) || 0, led1DynamicSteps);
              toggleVisibility(led1DynamicSteps, true);

              if (configData.led1.stepLengths && configData.led1.stepLengths.length > 0) {
                const inputs = led1DynamicSteps.getElementsByClassName("step-input");
                configData.led1.stepLengths.forEach((length, index) => {
                  if (inputs[index]) inputs[index].value = length;
                });
              }
            }
            toggleVisibility(led1Effects, true);
            document.getElementById("led1_rotacijaStepenica").checked = configData.led1?.rotation || false;
          }

          led1Effect.value = configData.led1?.effect || "";
          toggleVisibility(led1ColorPicker.parentElement, led1Effect.value === "solid");
          led1ColorPicker.value = configData.led1?.color || "#ffffff";
          document.getElementById("led1WipeSpeed").value = configData.led1?.wipeSpeed || "";
          document.getElementById("led1OnTime").value = configData.led1?.onTime || "";
          led1Brightness.value = configData.led1?.brightness || 100;
          led1BrightnessVal.innerText = led1Brightness.value;
          updateLed1EffectPreview();
        } else {
          toggleVisibility(led1ColorOrderSection, false);
        }

        // LED2
        led2Type.value = configData.led2?.type || "";
        led2ColorOrder.value = configData.led2?.colorOrder || "GRB";
        if (led2Type.value && led2Type.value !== "off") {
          toggleVisibility(led2ModeSection, true);
          toggleVisibility(led2ColorOrderSection, led2Type.value !== "ws2811_white");
          led2InstallationMode.value = configData.led2?.mode || "";

          if (led2InstallationMode.value === "linija") {
            toggleVisibility(led2LinijaSettings, true);
            document.getElementById("led2LinijaLEDCount").value = configData.led2?.linijaLEDCount || "";
            toggleVisibility(led2Effects, true);
          } else if (led2InstallationMode.value === "stepenica") {
            toggleVisibility(led2StepenicaSettings, true);
            led2NumSteps.value = configData.led2?.numSteps || "";
            led2DefaultLedsPerStep.value = configData.led2?.defaultLedsPerStep || "";
            led2VariableSteps.checked = configData.led2?.variable || false;

            if (configData.led2?.efektKreniOd === "ruba") {
              led2EfektKreniOdRuba.checked = true;
            } else {
              led2EfektKreniOdSredina.checked = true;
            }
            updateLed2EfektKreniOd();

            if (configData.led2?.stepMode === "sequence") {
              document.getElementById("led2_stepMode_sequence").checked = true;
            } else {
              document.getElementById("led2_stepMode_allAtOnce").checked = true;
            }

            if (led2VariableSteps.checked) {
              generateDynamicSteps(parseInt(led2NumSteps.value) || 0, parseInt(led2DefaultLedsPerStep.value) || 0, led2DynamicSteps);
              toggleVisibility(led2DynamicSteps, true);

              if (configData.led2.stepLengths && configData.led2.stepLengths.length > 0) {
                const inputs = led2DynamicSteps.getElementsByClassName("step-input");
                configData.led2.stepLengths.forEach((length, index) => {
                  if (inputs[index]) inputs[index].value = length;
                });
              }
            }
            toggleVisibility(led2Effects, true);
            document.getElementById("led2_rotacijaStepenica").checked = configData.led2?.rotation || false;
          }

          led2Effect.value = configData.led2?.effect || "";
          toggleVisibility(led2ColorPicker.parentElement, led2Effect.value === "solid");
          led2ColorPicker.value = configData.led2?.color || "#ffffff";
          document.getElementById("led2WipeSpeed").value = configData.led2?.wipeSpeed || "";
          document.getElementById("led2OnTime").value = configData.led2?.onTime || "";
          led2Brightness.value = configData.led2?.brightness || 100;
          led2BrightnessVal.innerText = led2Brightness.value;
          updateLed2EffectPreview();
        } else {
          toggleVisibility(led2ColorOrderSection, false);
        }
      })
      .catch(err => {
        console.error("Error fetching config:", err);
        showPopup("Greška pri učitavanju postavki!");
      });
  }

  function saveConfig() {
    const config = {
      mainSwitch: document.getElementById("mainSwitch").value,
      maxCurrent: parseInt(document.getElementById("maxCurrent").value) || 2000,
      led1: {
        type: led1Type.value,
        colorOrder: led1ColorOrder.value,
        mode: led1InstallationMode.value,
        linijaLEDCount: parseInt(document.getElementById("led1LinijaLEDCount").value) || 0,
        numSteps: parseInt(led1NumSteps.value) || 0,
        defaultLedsPerStep: parseInt(led1DefaultLedsPerStep.value) || 0,
        variable: led1VariableSteps.checked,
        stepLengths: [],
        efektKreniOd: document.querySelector('input[name="led1_efektKreniOd"]:checked').value,
        stepMode: document.querySelector('input[name="led1_stepMode"]:checked').value,
        rotation: document.getElementById("led1_rotacijaStepenica").checked,
        effect: led1Effect.value,
        color: led1ColorPicker.value,
        wipeSpeed: parseInt(document.getElementById("led1WipeSpeed").value) || 15,
        onTime: parseInt(document.getElementById("led1OnTime").value) || 5,
        brightness: parseInt(led1Brightness.value) || 100
      },
      led2: {
        type: led2Type.value,
        colorOrder: led2ColorOrder.value,
        mode: led2InstallationMode.value,
        linijaLEDCount: parseInt(document.getElementById("led2LinijaLEDCount").value) || 0,
        numSteps: parseInt(led2NumSteps.value) || 0,
        defaultLedsPerStep: parseInt(led2DefaultLedsPerStep.value) || 0,
        variable: led2VariableSteps.checked,
        stepLengths: [],
        efektKreniOd: document.querySelector('input[name="led2_efektKreniOd"]:checked').value,
        stepMode: document.querySelector('input[name="led2_stepMode"]:checked').value,
        rotation: document.getElementById("led2_rotacijaStepenica").checked,
        effect: led2Effect.value,
        color: led2ColorPicker.value,
        wipeSpeed: parseInt(document.getElementById("led2WipeSpeed").value) || 15,
        onTime: parseInt(document.getElementById("led2OnTime").value) || 5,
        brightness: parseInt(led2Brightness.value) || 100
      }
    };

    if (led1VariableSteps.checked) {
      const steps = led1DynamicSteps.getElementsByClassName("step-input");
      config.led1.stepLengths = Array.from(steps).map(input => parseInt(input.value) || 0);
    }

    if (led2VariableSteps.checked) {
      const steps = led2DynamicSteps.getElementsByClassName("step-input");
      config.led2.stepLengths = Array.from(steps).map(input => parseInt(input.value) || 0);
    }

    console.log("Saving config:", config);

    fetch("/api/saveConfig", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(config)
    })
      .then(response => response.json())
      .then(data => {
        console.log("Save response:", data);
        showPopup("Postavke spremljene!");
      })
      .catch(error => {
        console.error("Error saving config:", error);
        showPopup("Greška pri spremanju!");
      });
  }

  saveButtonLed1.addEventListener("click", saveConfig);
  saveButtonLed2.addEventListener("click", saveConfig);

  // Initial fetch of configuration
  fetchConfig();
});
