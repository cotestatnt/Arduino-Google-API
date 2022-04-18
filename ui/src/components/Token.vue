<template>
  <div>
    <BModal ref="BModal" :message="modalMessage" :title="modalTitle" :buttonType="modalButton" @Restart="doRestart" />
    <br><br>
    <div class="form-check" style="text-align: left; font-size: 0.85rem;">
      <input type="checkbox" class="form-check-input" v-model=verbose @click="toggleVerbose" />
      Show / Edit Google API OAuth2.0 credentials for this application
    </div>

    <div class="alert alert-primary" role="alert">
      <h4>Google APIs use the OAuth 2.0 protocol for authentication and authorization.</h4>
      Click to button below to start Google authorization flow.<br>
      Credentials data was loaded from default device, but you can change here if needed.<br>
    </div>

    <div v-if=verbose>
      <div class="input-group mb-3 sm">
        <div class="input-group-prepend"> <span class="input-group-text">Client ID</span> </div>
        <input type="text" class="form-control" v-model="client_id" >
      </div>

      <div class="input-group mb-3 sm">
        <div class="input-group-prepend"> <span class="input-group-text">Client Secret</span> </div>
        <input type="text" class="form-control" v-model="client_secret" >
      </div>

      <div class="input-group mb-3 sm">
        <br><div class="input-group-append"> <span class="input-group-text">API Key</span> </div>
        <input type="text" class="form-control" v-model="api_key" >
      </div>

      <div class="input-group mb-3 sm">
        <br><div class="input-group-append"> <span class="input-group-text">Client Scope</span> </div>
        <input type="text" class="form-control" v-model="scope" >
      </div>

      <div class="input-group mb-3 sm">
        <br><div class="input-group-append"> <span class="input-group-text">Redirect uri</span> </div>
        <input type="text" class="form-control" v-model="redirect_uri">
      </div>
      <br><br>
    </div>

    <div v-if="this.gotCode != true" style="margin: auto auto; margin-bottom: 40px;">
      <button type="button" class="btn btn-primary" @click="openPopup">Authorize the application for using Google APIs</button>
      <br>
    </div>

    <div v-if="this.gotCode != false && this.gotTokens != true" id="scroll-me">
      <div class="alert alert-primary" role="alert">
        Click button "Exchange authorization code for tokens" for ending authorization flow.
        <br>You will get back your own access token and refresh token for this application.<br><br>
        <button type="button" class="btn btn-primary" @click="getAuthCode">Exchange authorization code for tokens</button>
        <span class="blink" v-if="waitTokens">  ...loading</span>
      </div>
    </div>

    <div v-if="this.gotTokens != false">
      <div class="input-group mb-3">
        <br><div class="input-group-append"> <span class="input-group-text">Authorization Code</span> </div>
        <input type="text" class="form-control" placeholder="Authorization code..." v-model="authorization_code" readonly>
      </div>
      <pre><p><span v-html="jsonToSave"></span></p></pre>

      <div></div><br>
      <div class="input-group mb-3">
        <br><div class="input-group-append"> <span class="input-group-text">json Filename</span> </div>
        <input type="text" ref="json_filename" class="form-control" value="/gapi_config.json" readonly>
        <button class="btn btn-primary" type="button" @click="saveJSON">Save configuration file in flash memory!</button>
      </div><br>
    </div>
  </div>
</template>

<script>
import BModal from "../components/BModal";

export default {
  name: 'GetToken',

components: {
    BModal,
  },

  data() {
    return {
      modalButton: '',
      modalTitle: 'info',
      modalMessage: 'test message',
      verbose: false,
      client_id: this.value,
      client_secret: this.value,
      api_key: this.value,
      scope: this.value,
      authorization_code: this.value,
      authorization_uri: this.value,
      exchange_uri: 'https://oauth2.googleapis.com/token',
      redirect_uri: '',
      gotCode: false,
      waitTokens: false,
      gotTokens: false,
      jsonToSave: {},
      windowObjectReference : null
    };
  },

  mounted() {
    this.loadJson();
  },

  methods: {
    toggleVerbose() {
      var _MsgWindowOpenError = 'Change this data with your own application data if you need.\n\n'
          _MsgWindowOpenError += 'Configure you OAuth2.0 client in Google console in order to have\n';
          _MsgWindowOpenError += 'your own Client ID, Client secret, API key\n';
          _MsgWindowOpenError += 'If you don\'t have a valid redirection uri, leave unaltered.\n';

      if(!this.verbose)
        window.alert(_MsgWindowOpenError);
      this.verbose = !this.verbose;
    },


    openPopup() {
      var _MsgWindowOpenError = 'Please allow pop-up windows to authorize application.';
      if (window.name != "new") {
        var options = "fullscreen=no,height=800,width=600";
        var newWin = window.open(this.authorization_uri, "Authorize Smart App", options);

        if (newWin == null) {
          window.alert(_MsgWindowOpenError);
        }
        else if (window.top != newWin) {
          window.opener = window.self;
          this.gotCode = true;
          setTimeout(function() {
            var element = document.getElementById("scroll-me");
            element.scrollIntoView();
          }, 200 );

        }
      }
    },

    doRestart() {
      fetch('/restart')
        .then(response => console.log(response.text()))
        .catch(error => console.log('error', error));
      this.toggleModal();
      let url = 'http://' + location.host + '/setup';
      location.assign(url);
    },

    getAuthCode(){
      this.waitTokens = true;
      fetch(this.redirect_uri, { mode: 'cors'})
        .then(blob => blob.json())
        .then(data => {
          console.table(data);
          this.authorization_code = data['code'];
          this.getTokens = true;
          this.getGoogleTokens();
        })
        .catch(error => console.log('error', error));
    },

     getGoogleTokens() {
      var myHeaders = new Headers();
      myHeaders.append("Content-Type", "application/json");

      var payload = JSON.stringify({
        "client_id": this.client_id,
        "client_secret": this.client_secret,
        "grant_type": "authorization_code",
        "redirect_uri":  this.redirect_uri,
        "code": this.authorization_code
      });

      var requestOptions = {
        method: 'POST',
        headers: myHeaders,
        body: payload,
        redirect: 'follow'
      };

      fetch("https://oauth2.googleapis.com/token", requestOptions)
        .then(response => response.json())
        .then(obj => {
          // console.log(result)
          // let obj = JSON.parse(result);

          obj["client_id"] = this.client_id;
          obj["client_secret"] = this.client_secret;
          obj["api_key"] = this.api_key;
          obj["scope"] = this.scope;
          obj["redirect_uri"] = this.redirect_uri;

          this.jsonToSave = JSON.stringify(obj, null, 2);
          this.gotTokens = true;
        })
        .catch(error => console.log('error', error));
    },

    async loadJson() {
      fetch('/gapi_config.json')
        .then(response => response.json())
        .then(obj => {
          this.client_id = obj.client_id;
          this.client_secret = obj.client_secret;
          this.api_key = obj.api_key;
          this.scope = obj.scope;
          this.redirect_uri = obj.redirect_uri;
          this.authorization_uri =
          `https://accounts.google.com/o/oauth2/v2/auth?client_id=${this.client_id}&redirect_uri=${this.redirect_uri}&response_type=code&scope=${this.scope}&access_type=offline&prompt=consent`;
        })
        .catch(error => console.log('error', error));
    },

    saveJSON() {
      let path = this.$refs.json_filename.value;
      if (!path.startsWith("/"))
        path = "/" + path;

      var myblob = new Blob([this.jsonToSave], {
          type: 'application/json'
      });
      var formData = new FormData();
      formData.append("data", myblob, path);

      const headers = {
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Max-Age': '600',
        'Access-Control-Allow-Methods': 'PUT,POST,GET,OPTIONS',
        'Access-Control-Allow-Headers': '*'
      };

      var requestOptions = {
        method: 'POST',
        headers: headers,
        body: formData
      };

       fetch('/edit', requestOptions)
        .then(response => response.text())
        .then((result) => {
          this.modalTitle = 'Google API credentials'
          this.modalMessage = 'File <b>gapi_config.json</b> saved successfully!'
          this.modalButton =  'Restart';
          this.$refs.BModal.toggleModal();
          console.log(result);
        })
        .catch(error => console.log('error', error));
    },
  },
}

</script>

<!-- Add "scoped" attribute to limit CSS to this component only -->
<style scoped>
.auth-button{
  margin: 5% 10% 5% 10%;
  min-width: 40%;
}

.blink {
  animation: blinker 1s linear infinite;
}

@keyframes blinker {
  50% {
    opacity: 0;
  }
}
</style>
