﻿using System; using System.Threading.Tasks; using System.Web.UI;
namespace web_two {
    public partial class CRdt : System.Web.UI.Page {
        protected void Page_Load(object sender, EventArgs e) {
            if (!IsPostBack) RegisterAsyncTask(new PageAsyncTask(ExecuteSql));
        }
        protected void btnExecute_Click(object sender, EventArgs e) {
            RegisterAsyncTask(new PageAsyncTask(ExecuteSql));
        }
        private async Task ExecuteSql() {
            try {
                txtResult.Text = await DoSql();
            } catch (Exception err) {
                txtResult.Text = err.Message;
            }
        }
        private Task<string> DoSql() {
            TaskCompletionSource<string> tcs = new TaskCompletionSource<string>();
            string text = txtRentalId.Text;
            long rental_id = long.Parse(text);
            string sql = "SELECT rental_id,rental_date,return_date,last_update FROM rental where rental_id=" + rental_id;
            var handler = Global.Slave.SeekByQueue();
            if (handler == null) {
                tcs.SetResult("No connection to anyone of slave databases");
                return tcs.Task;
            }
            string s = "";
            bool ok = handler.Execute(sql, (h, res, errMsg, affected, fail_ok, vtId) => {
                if (res != 0) s = errMsg;
                tcs.SetResult(s);
            }, (h, vData) => {
                s = string.Format("rental_id={0}, rental={1}, return={2}, lastupdate={3}", vData[0], vData[1], vData[2], vData[3]);
            });
            /* you can asynchronously execute other SQL statements here and push results onto browsers
             * by ASP.NET SignalR to improve web response
             */
            return tcs.Task;
        }
    }
}