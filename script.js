document.addEventListener("DOMContentLoaded", () => {
  // -----------------------------
  // Translation Dictionary
  // -----------------------------
  const translations = {
    en: {
      title: "StairLight Configuration",
      led1Settings: "LED1 Settings",
      led2Settings: "LED2 Settings",
      dayNightSettings: "Day/Night Settings",
      ledType: "LED Type:",
      selectLEDType: "-- Select LED Type --",
      off: "Off",
      installationMode: "Installation Mode:",
      selectMode: "-- Select Mode --",
      linija: "Linija",
      stepenica: "Stepenica",
      numberOfLEDs: "Number of LEDs:",
      numberOfSteps: "Number of Steps:",
      defaultLEDsPerStep: "Default LEDs per Step:",
      variableStepLength: "Variable Step Length",
      efektKreniOd: "Efekt Kreni Od:",
      sredine: "Sredine",
      ruba: "Ruba",
      rotation: "Rotation: 180° every other",
      stepMode: "Step Mode:",
      allAtOnce: "All At Once",
      sequence: "Sequence",
      effect: "Effect:",
      selectEffect: "-- Select Effect --",
      solid: "Solid",
      confetti: "Confetti",
      rainbow: "Rainbow",
      meteor: "Meteor",
      fade: "Fade In/Out",
      fire: "Fire",
      lightning: "Lightning",
      wipeSpeed: "Wipe In/Out Speed (ms):",
      onTime: "On Time (sec):",
      preview: "Preview:",
      dayStart: "Day Starts (HH:MM):",
      dayEnd: "Day Ends (HH:MM):",
      dayBrightness: "Day Brightness:",
      nightBrightness: "Night Brightness:",
      globalSettings: "Global Settings",
      mainSwitch: "Main Switch:",
      maxCurrent: "Max Current (mA):"
    },
    hr: {
      title: "StairLight Konfiguracija",
      led1Settings: "Postavke LED1",
      led2Settings: "Postavke LED2",
      dayNightSettings: "Day/Night Postavke",
      ledType: "Tip LED-a:",
      selectLEDType: "-- Odaberi Tip LED-a --",
      off: "Isključi",
      installationMode: "Način instalacije:",
      selectMode: "-- Odaberi način --",
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
      selectEffect: "-- Odaberi Efekt --",
      solid: "Solid",
      confetti: "Konfeti",
      rainbow: "Duga",
      meteor: "Meteor",
      fade: "Fade In/Out",
      fire: "Vatra",
      lightning: "Munja",
      wipeSpeed: "Brzina brisanja (ms):",
      onTime: "Vrijeme uključenja (sec):",
      preview: "Pregled:",
      dayStart: "Dan počinje (HH:MM):",
      dayEnd: "Dan završava (HH:MM):",
      dayBrightness: "Svjetlina dana:",
      nightBrightness: "Svjetlina noći:",
      globalSettings: "Globalne Postavke",
      mainSwitch: "Glavni Prekidač:",
      maxCurrent: "Maksimalna struja (mA):"
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
    currentLang = (currentLang === "en") ? "hr" : "en";
    langToggle.textContent = currentLang.toUpperCase();
    updateTranslations();
  });

  // -----------------------------
  // Navigation
  // -----------------------------
  const led1Page = document.getElementById("led1Page");
  const led2Page = document.getElementById("led2Page");
  const dayNightPage = document.getElementById("dayNightPage");
  function showPage(pageId) {
    [led1Page, led2Page, dayNightPage].forEach(page => {
      page.style.display = "none";
    });
    document.getElementById(pageId).style.display = "block";
  }
  showPage("led1Page");
  document.getElementById("led1NavButton").addEventListener("click", () => { showPage("led1Page"); });
  document.getElementById("led2NavButton").addEventListener("click", () => { showPage("led2Page"); });
  document.getElementById("dayNightNavButton").addEventListener("click", () => { showPage("dayNightPage"); });

  // -----------------------------
  // Helper: Toggle Visibility
  // -----------------------------
  function toggleVisibility(el, show) {
    if (el) {
      el.classList.toggle("hidden", !show);
    }
  }

  // -----------------------------
  // Global Settings - mainSwitch and maxCurrent will be filled after fetching config.
  // -----------------------------
  
  // -----------------------------
  // LED1 Configuration
  // -----------------------------
  const led1Type = document.getElementById("led1Type");
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

  led1Type.addEventListener("change", () => {
    console.log("LED1 type changed:", led1Type.value);
    const show = led1Type.value !== "" && led1Type.value !== "off";
    toggleVisibility(led1ModeSection, show);
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

  // -----------------------------
  // LED2 Configuration
  // -----------------------------
  const led2Type = document.getElementById("led2Type");
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

  led2Type.addEventListener("change", () => {
    const show = led2Type.value !== "" && led2Type.value !== "off";
    toggleVisibility(led2ModeSection, show);
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

  // -----------------------------
  // Day/Night Configuration
  // -----------------------------
  const dayStart = document.getElementById("dayStart");
  const dayEnd = document.getElementById("dayEnd");
  const dayBr = document.getElementById("dayBr");
  const dayBrVal = document.getElementById("dayBrVal");
  const nightBr = document.getElementById("nightBr");
  const nightBrVal = document.getElementById("nightBrVal");

  dayBr.addEventListener("input", () => { if(dayBrVal) dayBrVal.innerText = dayBr.value; });
  nightBr.addEventListener("input", () => { if(nightBrVal) nightBrVal.innerText = nightBr.value; });

  // -----------------------------
  // Modal Popup
  // -----------------------------
  const popup = document.getElementById("popup");
  const closeBtn = document.querySelector(".close-button");
  closeBtn.addEventListener("click", () => { popup.style.display = "none"; });
  window.addEventListener("click", (ev) => { if (ev.target === popup) popup.style.display = "none"; });

  // -----------------------------
  // Fetch and Save Configuration
  // -----------------------------
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
        if (led1Type.value && led1Type.value !== "off") {
          toggleVisibility(led1ModeSection, true);
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
            
            // Efekt Kreni Od
            if (configData.led1?.efektKreniOd === "ruba") {
              document.getElementById("led1_efektKreniOd_ruba").checked = true;
            } else {
              document.getElementById("led1_efektKreniOd_sredina").checked = true;
            }
            updateLed1EfektKreniOd();
            
            // Step Mode
            if (configData.led1?.stepMode === "sequence") {
              document.getElementById("led1_stepMode_sequence").checked = true;
            } else {
              document.getElementById("led1_stepMode_allAtOnce").checked = true;
            }
            
            if (led1VariableSteps.checked) {
              generateDynamicSteps(configData.led1.numSteps, configData.led1.defaultLedsPerStep, led1DynamicSteps);
              toggleVisibility(led1DynamicSteps, true);
              
              // Postavi spremljene vrijednosti za svaki korak
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
          
          // Effect
          led1Effect.value = configData.led1?.effect || "";
          toggleVisibility(led1ColorPicker.parentElement, led1Effect.value === "solid");
          document.getElementById("led1Color").value = configData.led1?.color || "#ffffff";
          document.getElementById("led1WipeSpeed").value = configData.led1?.wipeSpeed || "";
          document.getElementById("led1OnTime").value = configData.led1?.onTime || "";
          updateLed1EffectPreview();
        }

        // LED2 (ista logika kao za LED1)
        led2Type.value = configData.led2?.type || "";
        if (led2Type.value && led2Type.value !== "off") {
          toggleVisibility(led2ModeSection, true);
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
            
            // Efekt Kreni Od
            if (configData.led2?.efektKreniOd === "ruba") {
              document.getElementById("led2_efektKreniOd_ruba").checked = true;
            } else {
              document.getElementById("led2_efektKreniOd_sredina").checked = true;
            }
            updateLed2EfektKreniOd();
            
            // Step Mode
            if (configData.led2?.stepMode === "sequence") {
              document.getElementById("led2_stepMode_sequence").checked = true;
            } else {
              document.getElementById("led2_stepMode_allAtOnce").checked = true;
            }
            
            if (led2VariableSteps.checked) {
              generateDynamicSteps(configData.led2.numSteps, configData.led2.defaultLedsPerStep, led2DynamicSteps);
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
          
          // Effect
          led2Effect.value = configData.led2?.effect || "";
          toggleVisibility(led2ColorPicker.parentElement, led2Effect.value === "solid");
          document.getElementById("led2Color").value = configData.led2?.color || "#ffffff";
          document.getElementById("led2WipeSpeed").value = configData.led2?.wipeSpeed || "";
          document.getElementById("led2OnTime").value = configData.led2?.onTime || "";
          updateLed2EffectPreview();
        }
        
        // Day/Night settings
        dayStart.value = configData.dayStart || "08:00";
        dayEnd.value = configData.dayEnd || "20:00";
        dayBr.value = configData.dayBr || 100;
        if(dayBrVal) dayBrVal.innerText = configData.dayBr || 100;
        nightBr.value = configData.nightBr || 30;
        if(nightBrVal) nightBrVal.innerText = configData.nightBr || 30;
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
        color: document.getElementById("led1Color").value,
        wipeSpeed: parseInt(document.getElementById("led1WipeSpeed").value) || 15,
        onTime: parseInt(document.getElementById("led1OnTime").value) || 5
      },
      led2: {
        type: led2Type.value,
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
        color: document.getElementById("led2Color").value,
        wipeSpeed: parseInt(document.getElementById("led2WipeSpeed").value) || 15,
        onTime: parseInt(document.getElementById("led2OnTime").value) || 5
      },
      dayStart: dayStart.value,
      dayEnd: dayEnd.value,
      dayBr: parseInt(dayBr.value),
      nightBr: parseInt(nightBr.value)
    };

    // Spremi duljine koraka ako je varijabilno uključeno
    if (led1VariableSteps.checked) {
      const steps = document.getElementById("led1DynamicSteps").getElementsByClassName("step-input");
      config.led1.stepLengths = Array.from(steps).map(input => parseInt(input.value) || 0);
    }

    if (led2VariableSteps.checked) {
      const steps = document.getElementById("led2DynamicSteps").getElementsByClassName("step-input");
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
  
  document.getElementById("saveConfigButton").addEventListener("click", saveConfig);
  
  function showPopup(msg) {
    const popupMsg = document.getElementById("popupMsg");
    popupMsg.innerText = msg;
    popup.style.display = "block";
  }
  
  // Finally, fetch the configuration.
  fetchConfig();
});
