(function() {
  const MAX_RETRIES = 3;
  const RETRY_DELAY = 1000; // ms
  const LOADED_ASSETS = new Set();

  function loadAsset(url, type, retryCount = 0) {
    if (LOADED_ASSETS.has(url)) return Promise.resolve();
    
    return new Promise((resolve, reject) => {
      let element;
      
      if (type === 'css') {
        element = document.createElement('link');
        element.rel = 'stylesheet';
        element.href = url;
      } else if (type === 'js') {
        element = document.createElement('script');
        element.src = url;
      }
      
      element.onload = () => {
        console.log(`Successfully loaded: ${url}`);
        LOADED_ASSETS.add(url);
        resolve();
      };
      
      element.onerror = () => {
        console.warn(`Failed to load ${url}, retry ${retryCount+1}/${MAX_RETRIES}`);
        
        if (retryCount < MAX_RETRIES) {
          if (element.parentNode) element.parentNode.removeChild(element);
          
          setTimeout(() => {
            loadAsset(url, type, retryCount + 1)
              .then(resolve)
              .catch(reject);
          }, RETRY_DELAY);
        } else {
          console.error(`Failed to load ${url} after ${MAX_RETRIES} attempts`);
          reject(new Error(`Failed to load ${url}`));
        }
      };
      
      document.head.appendChild(element);
    });
  }

  // Expose to global scope
  window.loadCSS = (url) => loadAsset(url, 'css');
  window.loadJS = (url) => loadAsset(url, 'js');
})();
