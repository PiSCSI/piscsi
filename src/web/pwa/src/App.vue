<template>
  <v-app>
    <v-app-bar
      app
      color="black"
      dark
    >
        <v-img
          alt="RaSCSI"
          class="shrink mr-2"
          contain
          :src="require('./assets/rascsi_logo2_scaled.png')"
          transition="scale-transition"
          width="40"
        />
        <v-spacer/>
        <v-chip-group >
            <v-chip small color="green">RaSCSI</v-chip>
            <v-chip small :color='index.netatalk_configured === 0 ? "gray" : "green"' >AFP</v-chip>
            <v-chip small color="gray">DaynaPort</v-chip>
        </v-chip-group>
    </v-app-bar>

    <v-main>
        <v-snackbar v-model="showError">
            <span v-for="(message, i) in flash.error" :key="i">
                    {{message}}
            </span>
            <template v-slot:action="{ attrs }">
                <v-btn
                    text
                    v-bind="attrs"
                    @click="flash.error = []"
                >
                    Close
                </v-btn>
            </template>
        </v-snackbar>
        <v-snackbar v-model="showMessage" color="success">
            <span v-for="(message, i) in flash.message" :key="i">
                {{message}}
            </span>
            <template v-slot:action="{ attrs }">
                <v-btn
                    icon
                    rounded
                    v-bind="attrs"
                    @click="flash.message = []"
                >
                    X
                </v-btn>
            </template>
        </v-snackbar>

        <v-card class="pt-2 pl-1 pr-1">
            <v-card-title class="pb-0 ">
                Devices
            </v-card-title>
            <v-data-table dense item-key="id" hide-default-footer sort-icon="false"
                :headers="deviceHeaders"
                :items="index.devices"/>
            <v-card-subtitle>
                <v-spacer/>
                <v-btn small color="primary" @click="detachAll()">Detach All</v-btn>
            </v-card-subtitle>
        </v-card>


        <v-card elevation="1" class="pt-2 pl-1 pr-1">
            <v-card-title class="pb-0">
                Files
            </v-card-title>
            <v-card-title class="pt-0">
                <v-text-field
                    class="pt-0"
                    v-model="search"
                    label="Search"
                    single-line
                    hide-details
                ></v-text-field>
            </v-card-title>
            <v-data-table :search="search" :items-per-page="5" single-expand show-expand
                          :items="index.files" item-key="name" :headers="fileHeaders" dense>
                <template v-slot:item.actions="{ item }">
                    <td>
                        <v-btn @click="attach(item)" small rounded icon ><v-icon>mdi-harddisk-plus</v-icon></v-btn>
                        <v-btn @click="deleteItem(item)" small rounded icon ><v-icon>mdi-delete-alert</v-icon></v-btn>
                        <v-btn @click="downloadItem(item)" small rounded icon ><v-icon>mdi-progress-download</v-icon></v-btn>
                    </td>
                </template>
                <template v-slot:expanded-item="{ headers, item }">
                    <td>
                        <v-select :items="validSCSIIds" v-model="recommendedSCSIId"></v-select>
                        <v-btn small @click="attach(item)">Attach</v-btn>
                    </td>
                    <td>
                        Type: {{item.detected_type}}
                    </td>
                </template>
            </v-data-table>
        </v-card>
<!--        <router-view/>-->
    </v-main>
      <v-footer>
          <v-flex>
              <small>
                  <pre class="text-center">{{index.running_env.env}}</pre>
                  <pre class="text-center">{{index.running_env.git}} - {{index.version}}</pre>
              </small>
          </v-flex>
      </v-footer>
  </v-app>
</template>

<script>

export default {
  name: 'App',

  data: () => ({
      search: "",
      flash: {
          error: [],
          message: []
      },
      index: {
          devices: [{id: 0}, {id: 1}, {id: 2}, {id: 3}, {id: 4}, {id: 5}, {id: 6}, {id: 7}],
          files: [],
          RESERVATIONS: [],
          reserved_scsi_ids: [],
          recommended_id: 6,
          running_env: {
              env: "",
              git: ""
          },
          version: "",
          netatalk_configured: 0, // number of netatalk processes running
      },
      deviceHeaders: [
          {
              text: 'ID',
              align: 'start',
              sortable: false,
              value: 'id',
          },
          {
              text: 'Type',
              align: 'start',
              sortable: false,
              value: 'type',
          },
          {
              text: 'File',
              align: 'start',
              sortable: false,
              value: 'file',
          },
          {
              text: 'Product',
              align: 'start',
              sortable: false,
              value: 'product',
          },
          {
              text: 'Status',
              align: 'start',
              sortable: false,
              value: 'status',
          },
      ],
      fileHeaders: [
          {
              text: 'Name',
              align: 'start',
              sortable: true,
              value: 'name',
          },
          {
              text: 'Actions',
              sortable: false,
              value: 'actions',
          },
          {
              text: 'Size',
              align: 'start',
              sortable: true,
              value: 'size_mb',
          },
      ]
  }),
  methods: {
      // TODO: Helper method for api calls (url, method, body)
      detachAll: function () {
          fetch('http://localhost:8080/scsi/detach_all', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json', 'Accept': 'application/json' },
          }).then(() => {
              this.loadIndex();
              this.flash.message = ["All devices detached"];
          }).catch(error => {
              this.flash.error.push(error);
          });
      },
      deleteItem: function (item) {
          console.alert("delete " + item.id);
      },
      downloadItem: function (item) {
          console.alert("download " + item.id);
      },
      attach: function(item) {
          fetch('http://localhost:8080/scsi/attach', {
              headers: { 'Content-Type': 'application/json', 'Accept': 'application/json' },
              method: "POST",
              body: JSON.stringify({...item, scsi_id: this.recommendedSCSIId})
          }).then(response => response.json())
          .then((data) => {
              this.loadIndex();
              this.flash = data.flash
          });
      },
      loadIndex() {
          fetch('http://localhost:8080/', {
              headers: { 'Accept': 'application/json' },
              method: "GET",
          }).then(response => response.json())
          .then(data => {
              this.index = data.data
          });
     },
  },
  computed: {
      showError() {
          return this.flash.error && this.flash.error.length > 0;
      },
      showMessage() {
          return this.flash.message && this.flash.message.length > 0;
      },
      validSCSIIds() {
          let scsi_ids = [0,1,2,3,4,5,6,7];
          const reserved = this.index.reserved_scsi_ids;
          // Cast Reserved to an integer array
          // FIXME: RESERVATIONS is currently a string array, should have ints.
          // const RESERVATIONS = this.index.RESERVATIONS.map(x => parseInt(x));
          // Merge the reserved and RESERVATIONS lists
          // const valid_scsi_ids = reserved.concat(RESERVATIONS);
          // Remove reserved SCSI IDs from the list of valid SCSI IDs
          const valid_scsi_ids = scsi_ids.filter(x => !reserved.includes(x));
          return valid_scsi_ids;
      },
      recommendedSCSIId() {
          return this.index.recommended_id;
      }
  },
  created() {
      this.loadIndex();
      setTimeout(() => {
          this.loadIndex()
      }, 5000);
  },

};
</script>

<style>
    .v-data-table > .v-data-table__wrapper .v-data-table__mobile-row {
        min-height: 0 !important;
    }
</style>
